export module PlaygroundEngine.Ecs.InputSystem;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.Ecs.InputSystem.InputStateComponent;
import PlaygroundEngine.PlatformEvents;

import std;

namespace PgE
{
	export class InputSystem : public System
	{
	public:
		explicit InputSystem(Ecs& ecs) : System(ecs)
		{}

		void UpdatePlatformInput(const PlatformEventRecord& platformEventRecord);
		void Step(float deltaTimeSeconds) override;

	private:
		std::shared_ptr<InputStateComponent> GetOrCreateInputState();

		PlatformEventRecord _currentPlatformEventRecord;
	};
}
