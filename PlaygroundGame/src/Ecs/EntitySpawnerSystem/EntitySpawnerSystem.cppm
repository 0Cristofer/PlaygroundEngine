export module PlaygroundGame.Ecs.EntitySpawnerSystem;

import PlaygroundEngine.Ecs;

import std;

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
		void DestroyFirstSpawnedEntity();
		void DestroyFirstSpawnedEntityName();
		PgE::Entity TakeFirstSpawnedEntity();

		// The spawner acts only on what it spawned. Reaching for "the first entity carrying a name" would
		// pick whichever entity has the lowest id, which is the app's own player long before it is anything
		// this system created.
		std::vector<PgE::Entity> _spawnedEntities;
		int _spawnCount = 0;
	};
}
