export module PlaygroundEngine.Ecs.InputSystem.InputStateComponent;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.PlatformEvents;

import std;

namespace PgE
{
	export enum class InputState
	{
		Released = 0,
		Pressed
	};

	export class InputStateComponent : public Component
	{
	public:
		using InputMap = std::unordered_map<InputCode, InputState>;

		InputMap CurrentInputMap;
		InputMap PreviousInputMap;

		[[nodiscard]] bool IsPressed(const InputCode code) const
		{
			return StateOf(CurrentInputMap, code) == InputState::Pressed;
		}

		[[nodiscard]] bool WasPressedThisFrame(const InputCode code) const
		{
			return StateOf(CurrentInputMap, code) == InputState::Pressed && StateOf(PreviousInputMap, code) == InputState::Released;
		}

		[[nodiscard]] bool WasReleasedThisFrame(const InputCode code) const
		{
			return StateOf(CurrentInputMap, code) == InputState::Released && StateOf(PreviousInputMap, code) == InputState::Pressed;
		}

		static InputState StateOf(const InputMap& inputMap, const InputCode code)
		{
			const auto entry = inputMap.find(code);
			return entry != inputMap.end() ? entry->second : InputState::Released;
		}
	};
}
