module PlaygroundGame.Ecs.CameraControlSystem;

import PlaygroundEngine.Ecs.InputSystem.InputStateComponent;
import PlaygroundEngine.PlatformEvents;
import PlaygroundGame.Ecs.FreeLookComponent;

import std;

namespace PgG
{
	namespace
	{
		// Short of the vertical, where ToEulerAngles stops recovering yaw and pitch separately.
		constexpr float PitchLimitRadians = 1.5f;
	}

	void CameraControlSystem::Step(const float deltaTimeSeconds)
	{
		const LookInput input = ReadLookInput();

		// Collected before mutating, so the system survives a storage that does not hand out snapshots.
		const std::vector<std::pair<PgE::Entity, std::shared_ptr<FreeLookComponent>>> lookers = GetComponentsWithEntities<FreeLookComponent>();

		for (const auto& [entity, freeLook] : lookers)
		{
			const std::shared_ptr<PgE::TransformComponent> transform = TryGetComponent<PgE::TransformComponent>(entity);
			if (transform == nullptr)
			{
				continue;
			}

			Turn(*transform, input, freeLook->TurnSpeedRadiansPerSecond, deltaTimeSeconds);
			Move(*transform, input, freeLook->MoveSpeedMetersPerSecond, deltaTimeSeconds);
		}
	}

	void CameraControlSystem::Turn(PgE::TransformComponent& transform,
								   const LookInput& input,
								   const float radiansPerSecond,
								   const float deltaTimeSeconds)
	{
		// The transform is the only place the rotation lives, so an untouched frame leaves whatever the
		// debug panel or another system wrote.

		if (input.Turn == 0.0f && input.Pitch == 0.0f)
		{
			return;
		}

		PgE::EulerAngles angles = transform.Rotation.ToEulerAngles();

		angles.Yaw += input.Turn * radiansPerSecond * deltaTimeSeconds;
		angles.Pitch = std::clamp(angles.Pitch + input.Pitch * radiansPerSecond * deltaTimeSeconds, -PitchLimitRadians, PitchLimitRadians);
		angles.Roll = 0.0f;

		transform.Rotation = PgE::Quaternion::FromEulerAngles(angles);
	}

	void CameraControlSystem::Move(PgE::TransformComponent& transform,
								   const LookInput& input,
								   const float metersPerSecond,
								   const float deltaTimeSeconds)
	{
		const PgE::Vector3 movement = transform.GetForward() * input.Forward + transform.GetRight() * input.Strafe;
		if (movement.LengthSquared() == 0.0f)
		{
			return;
		}

		// Normalized so a diagonal is not faster than an axis.
		transform.Position += PgE::Vector3::Normalize(movement) * metersPerSecond * deltaTimeSeconds;
	}

	CameraControlSystem::LookInput CameraControlSystem::ReadLookInput() const
	{
		const std::vector<std::shared_ptr<PgE::InputStateComponent>> inputStateComponents = GetComponents<PgE::InputStateComponent>();
		if (inputStateComponents.empty())
		{
			return LookInput{};
		}

		const PgE::InputStateComponent& inputState = *inputStateComponents.front();

		LookInput input;

		// Positive yaw turns left, so the left arrow drives Turn positively.

		if (inputState.IsPressed(PgE::InputCode::KeyLeft))
		{
			input.Turn += 1.0f;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyRight))
		{
			input.Turn -= 1.0f;
		}

		if (inputState.IsPressed(PgE::InputCode::KeyUp))
		{
			input.Pitch += 1.0f;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyDown))
		{
			input.Pitch -= 1.0f;
		}

		if (inputState.IsPressed(PgE::InputCode::KeyW))
		{
			input.Forward += 1.0f;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyS))
		{
			input.Forward -= 1.0f;
		}

		if (inputState.IsPressed(PgE::InputCode::KeyD))
		{
			input.Strafe += 1.0f;
		}
		if (inputState.IsPressed(PgE::InputCode::KeyA))
		{
			input.Strafe -= 1.0f;
		}

		return input;
	}
}
