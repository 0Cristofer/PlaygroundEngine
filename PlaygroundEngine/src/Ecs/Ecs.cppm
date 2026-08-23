export module PlaygroundEngine.Ecs;

export import :System;
export import PlaygroundEngine.Ecs.Component;
export import PlaygroundEngine.Ecs.Entity;
export import PlaygroundEngine.Ecs.PositionComponent;

import PlaygroundEngine.Reflection.Core;
import std;

namespace PgE
{
	export class Ecs
	{
	public:
		void AddSystem(std::unique_ptr<System> newSystem);

		Entity AddEntity();

		[[nodiscard]] bool IsEntityAlive(Entity entity) const;
		[[nodiscard]] std::vector<Entity> GetEntities() const;

		template <typename TComponent>
		std::shared_ptr<TComponent> AddComponentToEntity(const Entity entity) pre(IsEntityAlive(entity))
			// One component of a type per entity is what makes (type, entity) a key, so a second add is a
			// caller bug rather than a silent replacement of the first.
			pre(TryGetComponent<TComponent>(entity) == nullptr)
		{
			const auto entityEntry = _componentsPerEntity.find(entity.Id);
			if (entityEntry == _componentsPerEntity.end())
			{
				return nullptr;
			}

			std::shared_ptr<TComponent> component = std::make_shared<TComponent>();
			const ComponentRecord record{.Instance = component, .Ref = TypedRefOf(*component)};
			const TypeInfo* componentType = record.Ref.Type;

			ComponentsByEntity& componentsOfType = _componentsPerType[componentType];
			ComponentsByType& componentsOfEntity = entityEntry->second;

			componentsOfType[entity.Id] = record;
			componentsOfEntity[componentType] = record;

			return component;
		}

		template <typename TComponent>
		[[nodiscard]] std::shared_ptr<TComponent> TryGetComponent(const Entity entity) const
		{
			const ComponentRecord* record = FindComponent(&TypeMetaOf<TComponent>(), entity);
			if (record == nullptr)
			{
				return nullptr;
			}

			return std::static_pointer_cast<TComponent>(record->Instance);
		}

		template <typename TComponent>
		[[nodiscard]] std::vector<std::shared_ptr<TComponent>> GetComponents() const
		{
			std::vector<std::shared_ptr<TComponent>> components;

			for (const ComponentRecord& record : ComponentsOfType(&TypeMetaOf<TComponent>()) | std::views::values)
			{
				components.push_back(std::static_pointer_cast<TComponent>(record.Instance));
			}

			return components;
		}

		template <typename TComponent>
		[[nodiscard]] std::vector<std::pair<Entity, std::shared_ptr<TComponent>>> GetComponentsWithEntities() const
		{
			std::vector<std::pair<Entity, std::shared_ptr<TComponent>>> components;

			for (const auto& [entityId, record] : ComponentsOfType(&TypeMetaOf<TComponent>()))
			{
				components.emplace_back(Entity{.Id = entityId}, std::static_pointer_cast<TComponent>(record.Instance));
			}

			return components;
		}

		[[nodiscard]] std::vector<TypedRef> GetEntityComponents(Entity entity) const;

		void DestroyEntity(Entity entity) pre(IsEntityAlive(entity));

		template <typename TComponent>
		void DestroyComponent(const Entity entity) pre(IsEntityAlive(entity))
		{
			const TypeInfo* componentType = &TypeMetaOf<TComponent>();

			if (const auto typeEntry = _componentsPerType.find(componentType); typeEntry != _componentsPerType.end())
			{
				ComponentsByEntity& componentsOfType = typeEntry->second;
				componentsOfType.erase(entity.Id);
			}

			if (const auto entityEntry = _componentsPerEntity.find(entity.Id); entityEntry != _componentsPerEntity.end())
			{
				ComponentsByType& componentsOfEntity = entityEntry->second;
				componentsOfEntity.erase(componentType);
			}
		}

		void Step(float deltaTimeSeconds) const;

	private:
		struct ComponentRecord
		{
			std::shared_ptr<Component> Instance;

			// Carries the concrete type alongside a pointer to the most-derived object, so a consumer that
			// knows no component type (the debug panel) can still reach one through reflection.
			TypedRef Ref;
		};

		using ComponentsByEntity = std::map<int, ComponentRecord>;
		using ComponentsByType = std::map<const TypeInfo*, ComponentRecord>;

		[[nodiscard]] const ComponentsByEntity& ComponentsOfType(const TypeInfo* componentType) const;
		[[nodiscard]] const ComponentRecord* FindComponent(const TypeInfo* componentType, Entity entity) const;

		std::vector<std::unique_ptr<System>> _systems;

		std::map<const TypeInfo*, ComponentsByEntity> _componentsPerType;
		std::map<int, ComponentsByType> _componentsPerEntity;

		int _lastEntityId = Entity::InvalidId;
	};

	template <typename TComponent>
	std::shared_ptr<TComponent> System::AddComponentToEntity(const Entity entity)
	{
		return _ecs.AddComponentToEntity<TComponent>(entity);
	}

	template <typename TComponent>
	std::shared_ptr<TComponent> System::TryGetComponent(const Entity entity) const
	{
		return _ecs.TryGetComponent<TComponent>(entity);
	}

	template <typename TComponent>
	std::vector<std::shared_ptr<TComponent>> System::GetComponents() const
	{
		return _ecs.GetComponents<TComponent>();
	}

	template <typename TComponent>
	std::vector<std::pair<Entity, std::shared_ptr<TComponent>>> System::GetComponentsWithEntities() const
	{
		return _ecs.GetComponentsWithEntities<TComponent>();
	}

	template <typename TComponent>
	void System::DestroyComponent(const Entity entity) const
	{
		_ecs.DestroyComponent<TComponent>(entity);
	}
}
