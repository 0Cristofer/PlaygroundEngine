module PlaygroundGame.Ecs.MovementSystem;

import PlaygroundEngine.Ecs.InputSystem.InputStateComponent;
import PlaygroundEngine.PlatformEvents;
import PlaygroundGame.Ecs.PlayerControlledComponent;

import std;

namespace PgG
{
	void MovementSystem::Step(const float deltaTimeSeconds)
	{
		const PgE::Vector3 moveDirection = ReadMoveDirection();
		if (moveDirection == PgE::Vector3::Zero)
		{
			return;
		}

		// Collected before mutating, so the system survives a storage that does not hand out snapshots.
		const std::vector<std::pair<PgE::Entity, std::shared_ptr<PlayerControlledComponent>>> controlled =
			GetComponentsWithEntities<PlayerControlledComponent>();

		for (const auto& [entity, playerControlled] : controlled)
		{
			const std::shared_ptr<PgE::TransformComponent> transform = TryGetComponent<PgE::TransformComponent>(entity);
			if (transform == nullptr)
			{
				continue;
			}

			transform->Position += moveDirection * playerControlled->MoveSpeedMetersPerSecond * deltaTimeSeconds;
		}
	}

	PgE::Vector3 MovementSystem::ReadMoveDirection() const
	{
		const std::vector<std::shared_ptr<PgE::InputStateComponent>> inputStateComponents = GetComponents<PgE::InputStateComponent>();
		if (inputStateComponents.empty())
		{
			return PgE::Vector3::Zero;
		}

		const PgE::InputStateComponent& inputState = *inputStateComponents.front();

		// The world is Z up, so WASD drives the XY ground plane.
		PgE::Vector3 direction = PgE::Vector3::Zero;

		if (inputState.IsPressed(PgE::InputCode::KeyW))
		{
			direction += PgE::Vector3::Forward;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyS))
		{
			direction -= PgE::Vector3::Forward;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyD))
		{
			direction += PgE::Vector3::Right;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyA))
		{
			direction -= PgE::Vector3::Right;
		}

		if (direction == PgE::Vector3::Zero)
		{
			return direction;
		}

		// Normalized so a diagonal is not faster than an axis.
		return PgE::Vector3::Normalize(direction);
	}
}
