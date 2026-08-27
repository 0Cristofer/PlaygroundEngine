export module PlaygroundGame.Ecs.CameraControlSystem;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.Math;

namespace PgG
{
	export class CameraControlSystem : public PgE::System
	{
	public:
		explicit CameraControlSystem(PgE::Ecs& ecs) : System(ecs)
		{}

		void Step(float deltaTimeSeconds) override;

	private:
		struct LookInput
		{
			float Turn = 0.0f;
			float Pitch = 0.0f;
			float Forward = 0.0f;
			float Strafe = 0.0f;
		};

		[[nodiscard]] LookInput ReadLookInput() const;

		static void Turn(PgE::TransformComponent& transform, const LookInput& input, float radiansPerSecond, float deltaTimeSeconds);
		static void Move(PgE::TransformComponent& transform, const LookInput& input, float metersPerSecond, float deltaTimeSeconds);
	};
}
