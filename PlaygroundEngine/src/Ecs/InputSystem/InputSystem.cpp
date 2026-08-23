module PlaygroundEngine.Ecs.InputSystem;

namespace PgE
{
	void InputSystem::UpdatePlatformInput(const PlatformEventRecord& platformEventRecord)
	{
		_currentPlatformEventRecord = platformEventRecord;
	}

	void InputSystem::Step(float)
	{
		const std::shared_ptr<InputStateComponent> inputState = GetOrCreateInputState();

		inputState->PreviousInputMap = inputState->CurrentInputMap;

		for (const PlatformEvent& platformEvent : _currentPlatformEventRecord.GetEvents())
		{
			if (IsKeyboardCode(platformEvent.Code))
			{
				inputState->CurrentInputMap[platformEvent.Code] =
					platformEvent.Type == PlatformEventType::KeyPressed ? InputState::Pressed : InputState::Released;
			}
			else if (IsPointerCode(platformEvent.Code))
			{
				inputState->CurrentInputMap[platformEvent.Code] =
					platformEvent.Type == PlatformEventType::PointerButtonPressed ? InputState::Pressed : InputState::Released;
			}
		}
	}

	std::shared_ptr<InputStateComponent> InputSystem::GetOrCreateInputState()
	{
		if (const std::vector<std::shared_ptr<InputStateComponent>> components = GetComponents<InputStateComponent>(); !components.empty())
		{
			return components.front();
		}

		return AddComponentToEntity<InputStateComponent>(AddEntity());
	}
}
