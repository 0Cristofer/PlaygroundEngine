export module PlaygroundGame.Ecs.EntitySpawnerSystem;

import PlaygroundEngine.Ecs;

namespace PgG
{
	export class EntitySpawnerSystem : public PgE::System
	{
	public:
		explicit EntitySpawnerSystem(PgE::Ecs& ecs) : System(ecs)
		{}

		void Step(float deltaTimeSeconds) override;

	private:
		void SpawnEntity();
		void DestroyFirstNamedEntity();
		void DestroyFirstEntityName() const;

		int _spawnCount = 0;
	};
}
