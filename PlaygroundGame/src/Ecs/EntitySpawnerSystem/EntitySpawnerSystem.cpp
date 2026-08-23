module PlaygroundGame.Ecs.EntitySpawnerSystem;

import PlaygroundEngine.Ecs.InputSystem.InputStateComponent;
import PlaygroundGame.Ecs.EntityNameComponent;
import PlaygroundEngine.PlatformEvents;

import std;

namespace PgG
{
	void EntitySpawnerSystem::Step(float)
	{
		const std::vector<std::shared_ptr<PgE::InputStateComponent>> inputStateComponents = GetComponents<PgE::InputStateComponent>();
		if (inputStateComponents.empty())
		{
			return;
		}

		const std::shared_ptr<PgE::InputStateComponent>& inputState = inputStateComponents.front();

		if (inputState->WasPressedThisFrame(PgE::InputCode::KeyP))
		{
			SpawnEntity();
		}
		if (inputState->WasPressedThisFrame(PgE::InputCode::KeyL))
		{
			DestroyFirstNamedEntity();
		}
		if (inputState->WasPressedThisFrame(PgE::InputCode::KeyM))
		{
			DestroyFirstEntityName();
		}
	}

	void EntitySpawnerSystem::SpawnEntity()
	{
		const PgE::Entity entity = AddEntity();

		const std::shared_ptr<EntityNameComponent> name = AddComponentToEntity<EntityNameComponent>(entity);
		name->Name = std::format("PlaygroundGame {}", ++_spawnCount);

		AddComponentToEntity<PgE::PositionComponent>(entity);
	}

	void EntitySpawnerSystem::DestroyFirstNamedEntity()
	{
		const std::vector<std::pair<PgE::Entity, std::shared_ptr<EntityNameComponent>>> named = GetComponentsWithEntities<EntityNameComponent>();
		if (named.empty())
		{
			return;
		}

		const auto& [entity, name] = named.front();
		DestroyEntity(entity);
	}

	void EntitySpawnerSystem::DestroyFirstEntityName() const
	{
		const std::vector<std::pair<PgE::Entity, std::shared_ptr<EntityNameComponent>>> named = GetComponentsWithEntities<EntityNameComponent>();
		if (named.empty())
		{
			return;
		}

		const auto& [entity, name] = named.front();
		DestroyComponent<EntityNameComponent>(entity);
	}
}
