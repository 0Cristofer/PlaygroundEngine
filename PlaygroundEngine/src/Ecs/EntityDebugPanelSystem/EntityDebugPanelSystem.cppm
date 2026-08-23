export module PlaygroundGame.Ecs.EntityDebugPanelSystem;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.DebugUi;

import imgui;

namespace PgG
{
	export class EntityDebugPanelSystem : public PgE::System
	{
	public:
		explicit EntityDebugPanelSystem(PgE::Ecs& ecs) : System(ecs)
		{}

		void Step(float deltaTimeSeconds) override;
	};
}
