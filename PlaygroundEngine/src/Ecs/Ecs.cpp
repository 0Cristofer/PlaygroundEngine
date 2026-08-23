module PlaygroundEngine.Ecs;

namespace PgE
{
	void Ecs::AddSystem(std::unique_ptr<System> newSystem)
	{
		_systems.push_back(std::move(newSystem));
	}

	Entity Ecs::AddEntity()
	{
		const Entity entity{.Id = ++_lastEntityId};

		_componentsPerEntity.try_emplace(entity.Id);

		return entity;
	}

	bool Ecs::IsEntityAlive(const Entity entity) const
	{
		return _componentsPerEntity.contains(entity.Id);
	}

	std::vector<Entity> Ecs::GetEntities() const
	{
		std::vector<Entity> entities;
		entities.reserve(_componentsPerEntity.size());

		for (const auto& entityId : _componentsPerEntity | std::views::keys)
		{
			entities.push_back(Entity{.Id = entityId});
		}

		return entities;
	}

	std::vector<TypedRef> Ecs::GetEntityComponents(const Entity entity) const
	{
		std::vector<TypedRef> components;

		const auto entityEntry = _componentsPerEntity.find(entity.Id);
		if (entityEntry == _componentsPerEntity.end())
		{
			return components;
		}

		const ComponentsByType& componentsOfEntity = entityEntry->second;

		components.reserve(componentsOfEntity.size());
		for (const ComponentRecord& record : componentsOfEntity | std::views::values)
		{
			components.push_back(record.Ref);
		}

		return components;
	}

	void Ecs::DestroyEntity(const Entity entity)
	{
		const auto entityEntry = _componentsPerEntity.find(entity.Id);
		if (entityEntry == _componentsPerEntity.end())
		{
			return;
		}

		for (const ComponentsByType& componentsOfEntity = entityEntry->second; const TypeInfo* componentType : componentsOfEntity | std::views::keys)
		{
			if (const auto typeEntry = _componentsPerType.find(componentType); typeEntry != _componentsPerType.end())
			{
				ComponentsByEntity& componentsOfType = typeEntry->second;
				componentsOfType.erase(entity.Id);
			}
		}

		_componentsPerEntity.erase(entityEntry);
	}

	void Ecs::Step(const float deltaTimeSeconds) const
	{
		for (const std::unique_ptr<System>& system : _systems)
		{
			system->Step(deltaTimeSeconds);
		}
	}

	const Ecs::ComponentsByEntity& Ecs::ComponentsOfType(const TypeInfo* componentType) const
	{
		// A type nothing was ever added for reads as empty rather than inserting a bucket for it, which is
		// what keeps every query const.
		static const ComponentsByEntity None;

		const auto typeEntry = _componentsPerType.find(componentType);
		return typeEntry != _componentsPerType.end() ? typeEntry->second : None;
	}

	const Ecs::ComponentRecord* Ecs::FindComponent(const TypeInfo* componentType, const Entity entity) const
	{
		const ComponentsByEntity& components = ComponentsOfType(componentType);

		const auto entityEntry = components.find(entity.Id);
		return entityEntry != components.end() ? &entityEntry->second : nullptr;
	}
}
