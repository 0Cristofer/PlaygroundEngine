#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Ecs;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ContractSeam;

namespace
{
	class NameComponent : public PgE::Component
	{
	public:
		std::string Name;
	};

	class CounterComponent : public PgE::Component
	{
	public:
		int Count = 0;
	};

	std::vector<int> IdsOf(const std::vector<PgE::Entity>& entities)
	{
		std::vector<int> ids;
		ids.reserve(entities.size());

		for (const PgE::Entity entity : entities)
		{
			ids.push_back(entity.Id);
		}

		return ids;
	}
}

TEST_CASE("a component is reachable from the entity that owns it")
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();

	const std::shared_ptr<NameComponent> added = ecs.AddComponentToEntity<NameComponent>(entity);
	added->Name = "named";

	// The join primitive: every real system pairs two component types on one entity through this.
	const std::shared_ptr<NameComponent> found = ecs.TryGetComponent<NameComponent>(entity);
	REQUIRE(found != nullptr);
	CHECK(found == added);
	CHECK(found->Name == "named");

	// A type the entity does not carry answers null rather than default-constructing one.
	CHECK(ecs.TryGetComponent<CounterComponent>(entity) == nullptr);
}

// Marked should_fail, not skipped or deleted: the contracts these two cases exercise are written the way
// the standard says, and GCC drops a pre-written in a module interface when the function is a template or
// is defined in the implementation unit (see the probe in docs/CorrectnessAndStandards.md).
TEST_CASE("an entity carries at most one component of a type" * doctest::should_fail())
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();

	ecs.AddComponentToEntity<NameComponent>(entity);

	CHECK_THROWS_AS(ecs.AddComponentToEntity<NameComponent>(entity), PgETest::ContractViolationError);
}

TEST_CASE("an entity with no components is still enumerable and destroyable")
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();

	CHECK(ecs.IsEntityAlive(entity));
	CHECK(IdsOf(ecs.GetEntities()) == std::vector{entity.Id});
	CHECK(ecs.GetEntityComponents(entity).empty());

	ecs.DestroyEntity(entity);

	CHECK_FALSE(ecs.IsEntityAlive(entity));
	CHECK(ecs.GetEntities().empty());
}

TEST_CASE("destroying an entity removes it from every component query")
{
	PgE::Ecs ecs;
	const PgE::Entity kept = ecs.AddEntity();
	const PgE::Entity destroyed = ecs.AddEntity();

	ecs.AddComponentToEntity<NameComponent>(kept);
	ecs.AddComponentToEntity<NameComponent>(destroyed);
	ecs.AddComponentToEntity<CounterComponent>(destroyed);

	ecs.DestroyEntity(destroyed);

	CHECK(ecs.GetComponents<NameComponent>().size() == 1);
	CHECK(ecs.GetComponents<CounterComponent>().empty());
	CHECK(ecs.TryGetComponent<NameComponent>(kept) != nullptr);
	CHECK(IdsOf(ecs.GetEntities()) == std::vector{kept.Id});
}

TEST_CASE("destroying one component leaves the entity and its other components")
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();

	ecs.AddComponentToEntity<NameComponent>(entity);
	ecs.AddComponentToEntity<CounterComponent>(entity);

	ecs.DestroyComponent<NameComponent>(entity);

	CHECK(ecs.IsEntityAlive(entity));
	CHECK(ecs.TryGetComponent<NameComponent>(entity) == nullptr);
	CHECK(ecs.TryGetComponent<CounterComponent>(entity) != nullptr);
	CHECK(ecs.GetEntityComponents(entity).size() == 1);

	ecs.DestroyComponent<NameComponent>(entity);
	CHECK(ecs.IsEntityAlive(entity));
}

TEST_CASE("a handle to a destroyed entity is rejected" * doctest::should_fail())
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();
	ecs.DestroyEntity(entity);

	CHECK_THROWS_AS(ecs.DestroyEntity(entity), PgETest::ContractViolationError);
	CHECK_THROWS_AS(ecs.AddComponentToEntity<NameComponent>(entity), PgETest::ContractViolationError);
	CHECK_THROWS_AS(ecs.DestroyComponent<NameComponent>(entity), PgETest::ContractViolationError);
}

TEST_CASE("a query about a destroyed entity answers empty")
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();
	ecs.DestroyEntity(entity);

	// Asking about a dead entity is a question, not misuse, so it carries no contract and answers empty
	// whether the guards above are running.
	CHECK_FALSE(ecs.IsEntityAlive(entity));
	CHECK(ecs.TryGetComponent<NameComponent>(entity) == nullptr);
	CHECK(ecs.GetEntityComponents(entity).empty());
	CHECK(ecs.GetEntities().empty());
}

TEST_CASE("queries come back in entity id order, not in allocation order")
{
	PgE::Ecs ecs;
	std::vector<PgE::Entity> entities;

	for (int index = 0; index < 5; ++index)
	{
		const PgE::Entity entity = ecs.AddEntity();
		ecs.AddComponentToEntity<CounterComponent>(entity)->Count = entity.Id;
		entities.push_back(entity);
	}

	// Destroying from the middle is what would scramble a container keyed on addresses, since the freed
	// slots are what the next allocations reuse.
	ecs.DestroyEntity(entities[1]);
	ecs.DestroyEntity(entities[3]);

	const PgE::Entity added = ecs.AddEntity();
	ecs.AddComponentToEntity<CounterComponent>(added)->Count = added.Id;

	std::vector<int> counts;
	for (const auto& [entity, component] : ecs.GetComponentsWithEntities<CounterComponent>())
	{
		CHECK(entity.Id == component->Count);
		counts.push_back(component->Count);
	}

	CHECK(counts == std::vector{1, 3, 5, 6});
	CHECK(IdsOf(ecs.GetEntities()) == std::vector{1, 3, 5, 6});
}

TEST_CASE("an entity's components are reachable without naming their types")
{
	PgE::Ecs ecs;
	const PgE::Entity entity = ecs.AddEntity();

	ecs.AddComponentToEntity<NameComponent>(entity)->Name = "erased";
	ecs.AddComponentToEntity<CounterComponent>(entity)->Count = 7;

	// What the debug panel walks: the borrow carries the concrete type, captured where it was still
	// statically known, so reflection reads the derived object rather than the Component base.
	std::string readBack;
	int typesSeen = 0;

	for (const PgE::TypedRef& component : ecs.GetEntityComponents(entity))
	{
		++typesSeen;

		if (component.Type == &PgE::TypeMetaOf<NameComponent>())
		{
			REQUIRE(component.Type->GetFieldValue(component, "Name", PgE::TypedRefOf(readBack)).has_value());
		}
	}

	CHECK(typesSeen == 2);
	CHECK(readBack == "erased");
}
