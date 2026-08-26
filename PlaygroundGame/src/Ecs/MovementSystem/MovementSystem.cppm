export module PlaygroundGame.Ecs.MovementSystem;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.Math;

namespace PgG
{
	export class MovementSystem : public PgE::System
	{
	public:
		explicit MovementSystem(PgE::Ecs& ecs) : System(ecs)
		{}

		void Step(float deltaTimeSeconds) override;

	private:
		PgE::Vector3 ReadMoveDirection() const;
	};
}
