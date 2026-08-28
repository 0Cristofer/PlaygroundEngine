module PlaygroundGame.Ecs.EntitySpawnerSystem;

import PlaygroundEngine.Ecs.InputSystem.InputStateComponent;
import PlaygroundGame.Ecs.EntityNameComponent;
import PlaygroundEngine.Ecs.MeshComponent;
import PlaygroundEngine.Math;
import PlaygroundEngine.PlatformEvents;

import std;

namespace PgG
{
	constexpr float SpawnSpacingMeters = 1.5f;

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
			DestroyFirstSpawnedEntity();
		}
		if (inputState->WasPressedThisFrame(PgE::InputCode::KeyM))
		{
			DestroyFirstSpawnedEntityName();
		}
	}

	void EntitySpawnerSystem::SpawnEntity()
	{
		const PgE::Entity entity = AddEntity();

		const int spawnOrdinal = ++_spawnCount;

		const std::shared_ptr<EntityNameComponent> name = AddComponentToEntity<EntityNameComponent>(entity);
		name->Name = std::format("PlaygroundGame {}", spawnOrdinal);

		// Spread along +Y so successive presses line up rather than stacking on the origin.

		const std::shared_ptr<PgE::TransformComponent> transform = AddComponentToEntity<PgE::TransformComponent>(entity);
		transform->Position = PgE::Vector3{.X = 0.0f, .Y = static_cast<float>(spawnOrdinal) * SpawnSpacingMeters, .Z = 0.0f};

		AddComponentToEntity<PgE::MeshComponent>(entity);

		_spawnedEntities.push_back(entity);
	}

	// A spawned entity may already be gone (destroyed from elsewhere, or by a previous press), so the
	// forgotten ones are dropped until a live one turns up.
	PgE::Entity EntitySpawnerSystem::TakeFirstSpawnedEntity()
	{
		while (!_spawnedEntities.empty())
		{
			const PgE::Entity entity = _spawnedEntities.front();
			_spawnedEntities.erase(_spawnedEntities.begin());

			if (IsEntityAlive(entity))
			{
				return entity;
			}
		}

		return PgE::Entity{};
	}

	void EntitySpawnerSystem::DestroyFirstSpawnedEntity()
	{
		if (const PgE::Entity entity = TakeFirstSpawnedEntity(); entity.Id != PgE::Entity::InvalidId)
		{
			DestroyEntity(entity);
		}
	}

	void EntitySpawnerSystem::DestroyFirstSpawnedEntityName()
	{
		if (const PgE::Entity entity = TakeFirstSpawnedEntity(); entity.Id != PgE::Entity::InvalidId)
		{
			DestroyComponent<EntityNameComponent>(entity);
		}
	}
}
