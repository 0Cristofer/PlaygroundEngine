module PlaygroundEngine.Ecs;

namespace PgE
{
	System::System(Ecs& ecs) : _ecs(ecs)
	{}

	Entity System::AddEntity() const
	{
		return _ecs.AddEntity();
	}

	bool System::IsEntityAlive(const Entity entity) const
	{
		return _ecs.IsEntityAlive(entity);
	}

	std::vector<Entity> System::GetEntities() const
	{
		return _ecs.GetEntities();
	}

	std::vector<TypedRef> System::GetEntityComponents(const Entity entity) const
	{
		return _ecs.GetEntityComponents(entity);
	}

	void System::DestroyEntity(const Entity entity) const
	{
		_ecs.DestroyEntity(entity);
	}
}
