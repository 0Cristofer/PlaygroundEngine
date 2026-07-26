#pragma once

// Member injection: generating data members from reflected information with a consteval block and
// std::meta::define_aggregate, the only code injection in C++26. Findings, constraints and design
// implications are in docs/ReflectionInjection.md.

struct Particle { float positionX; float positionY; int generation; };

// substitute() instantiates a template at consteval time, so a generated member's type can depend on
// the reflected field's type. That is what keeps the result statically typed instead of erased.
template <typename FieldType>
struct FieldData
{
	using Type = FieldType;

	std::string_view name;
	int byteOffset;
};

template <typename T>
class Fields
{
public:
	// Nested, so it shares this template's scope. define_aggregate rejects a target separated from the
	// block by an intervening scope, and satisfying it here is what makes injection automatic per T,
	// with no registration site.
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(std::meta::substitute(^^FieldData, {std::meta::type_of(member)}),
											{.name = std::meta::identifier_of(member)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}

	static constexpr auto Members =
		std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));

private:
	// Injected members are positional and cannot carry default initializers, so filling them generically
	// takes one aggregate initializer with the pack expanded over every index. Must be declared before
	// the static member that calls it; unqualified lookup will not find it afterwards.
	template <std::size_t... MemberIndices>
	static consteval Storage BuildStorage(std::index_sequence<MemberIndices...>)
	{
		return Storage{
			typename[:std::meta::substitute(^^FieldData, {std::meta::type_of(Members[MemberIndices])}):]{
				.name = std::meta::identifier_of(Members[MemberIndices]),
				.byteOffset = static_cast<int>(std::meta::offset_of(Members[MemberIndices]).bytes)}...};
	}

public:
	static constexpr Storage Of = BuildStorage(std::make_index_sequence<Members.size()>{});
};

// Each field becomes std::array<Field, N>. reflect_constant turns N into a usable template argument.
template <typename T, std::size_t N>
class StructureOfArrays
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			memberSpecs.push_back(std::meta::data_member_spec(
				std::meta::substitute(^^std::array, {std::meta::type_of(member), std::meta::reflect_constant(N)}),
				{.name = std::meta::identifier_of(member)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

// Each field becomes std::optional<Field>: a partial update. Serves a network delta and an editor
// multi-select grid equally, where "unset" means the values differ across the selection.
template <typename T>
class Patch
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(std::meta::substitute(^^std::optional, {std::meta::type_of(member)}),
											{.name = std::meta::identifier_of(member)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

// One 1-bit field per member, so three fields cost one byte. Named flags (dirty.generation) beat an
// index-keyed bitset for debuggability, which is what a replication layer would otherwise carry.
template <typename T>
class DirtyFlags
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(^^bool, {.name = std::meta::identifier_of(member), .bit_width = 1}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

struct Vector3 { float x, y, z; };

struct CameraUniforms { float nearPlane; Vector3 position; float farPlane; Vector3 forward; };
struct PackedCameraUniforms { Vector3 position; float nearPlane; Vector3 forward; float farPlane; };

consteval std::size_t Std140AlignmentOf(const std::meta::info type)
{
	// std140 rounds vectors and aggregates up to 16; scalars keep their natural alignment.
	if (std::meta::is_class_type(type))
	{
		return 16;
	}

	return std::meta::alignment_of(type);
}

// Not a replacement for the hand-written block but an oracle for it: generate the layout the GPU
// expects, then assert the struct actually uploaded agrees, turning a silent CPU/GPU layout mismatch
// into a compile error.
template <typename T>
class Std140
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(std::meta::type_of(member),
											{.name = std::meta::identifier_of(member),
											 .alignment = Std140AlignmentOf(std::meta::type_of(member))}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

template <typename T>
consteval bool MatchesStd140()
{
	using MirrorType = typename Std140<T>::Storage;

	constexpr auto nativeMembers =
		std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
	constexpr auto mirrorMembers = std::define_static_array(
		std::meta::nonstatic_data_members_of(^^MirrorType, std::meta::access_context::unchecked()));

	if (sizeof(T) != sizeof(MirrorType))
	{
		return false;
	}

	for (std::size_t index = 0; index < nativeMembers.size(); ++index)
	{
		if (std::meta::offset_of(nativeMembers[index]).bytes != std::meta::offset_of(mirrorMembers[index]).bytes)
		{
			return false;
		}
	}

	return true;
}

struct Messy { char flag; double scale; char kind; int count; char extra; };

// Same fields sorted by descending alignment, dropping the padding declaration order forced. Only safe
// where the generated type IS the storage: every offset changes, so offset arithmetic or a memcpy
// between T and the compacted form breaks silently.
template <typename T>
class Compact
{
public:
	struct Storage;

	consteval
	{
		auto members = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked());

		// stable_sort allocates a temporary buffer and so is unusable during constant evaluation.
		std::ranges::sort(members, std::greater<>{}, [](const std::meta::info member) {
			return std::meta::alignment_of(std::meta::type_of(member));
		});

		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member : members)
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(std::meta::type_of(member), {.name = std::meta::identifier_of(member)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

// Reuses the [[=Replicated{}]] tag from annotation_registry.h, which already means exactly this.
struct Health { [[=Replicated{}]] int current; [[=Replicated{}]] int maximum; float regenerationTimer; };

// Only annotated fields survive, so the wire payload becomes a type that physically cannot carry an
// unreplicated field: a type-level guarantee rather than a runtime filter to be trusted.
template <typename T>
class ReplicatedFields
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info member :
			 std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
		{
			if (!std::meta::annotations_of_with_type(member, ^^Replicated).empty())
			{
				memberSpecs.push_back(std::meta::data_member_spec(
					std::meta::type_of(member), {.name = std::meta::identifier_of(member)}));
			}
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

enum class EventKind : std::uint8_t { Spawn, Despawn, Damage, Heal };

// The name source need not be a struct's fields. The same shape works over member functions, but only
// until two overloads collide: injected names must be unique identifiers, the ceiling on the technique.
template <typename EnumType, typename SlotType>
class PerEnumerator
{
public:
	struct Storage;

	consteval
	{
		std::vector<std::meta::info> memberSpecs;
		for (const std::meta::info enumerator : std::meta::enumerators_of(^^EnumType))
		{
			memberSpecs.push_back(
				std::meta::data_member_spec(^^SlotType, {.name = std::meta::identifier_of(enumerator)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}
};

// -----------------------------------------------------------------------------
// Demo
// -----------------------------------------------------------------------------
void DemoMemberInjection()
{
	std::cout << "\n=== Member Injection ===\n";

	// Real named members, so Of.positionX autocompletes and a typo is a compile error.
	std::cout << "  Fields<Particle>: " << Fields<Particle>::Of.positionX.name << " @"
			  << Fields<Particle>::Of.positionX.byteOffset << ", " << Fields<Particle>::Of.generation.name << " @"
			  << Fields<Particle>::Of.generation.byteOffset << "\n";
	static_assert(std::is_same_v<decltype(Fields<Particle>::Of.generation)::Type, int>);

	StructureOfArrays<Particle, 4>::Storage particles{};
	particles.positionX[2] = 1.5f;
	static_assert(std::is_same_v<decltype(particles.positionX), std::array<float, 4>>);
	std::cout << "  StructureOfArrays<Particle, 4>: sizeof=" << sizeof(particles) << ", positionX[2]="
			  << particles.positionX[2] << "\n";

	Patch<Particle>::Storage patch{};
	patch.positionY = 3.f;
	std::cout << "  Patch<Particle>: positionY set=" << patch.positionY.has_value() << ", positionX set="
			  << patch.positionX.has_value() << "\n";

	DirtyFlags<Particle>::Storage dirty{};
	dirty.generation = true;
	const bool generationDirty = dirty.generation;   // a bitfield cannot bind to a reference
	std::cout << "  DirtyFlags<Particle>: sizeof=" << sizeof(dirty) << " for 3 fields, generation="
			  << generationDirty << "\n";

	std::cout << "  std140: CameraUniforms matches=" << MatchesStd140<CameraUniforms>() << " (native "
			  << sizeof(CameraUniforms) << ", std140 " << sizeof(Std140<CameraUniforms>::Storage)
			  << "), PackedCameraUniforms matches=" << MatchesStd140<PackedCameraUniforms>() << "\n";
	static_assert(MatchesStd140<PackedCameraUniforms>(), "upload layout disagrees with std140");

	std::cout << "  Compact<Messy>: " << sizeof(Messy) << " -> " << sizeof(Compact<Messy>::Storage) << " bytes\n";

	const ReplicatedFields<Health>::Storage wire{.current = 10, .maximum = 100};
	std::cout << "  ReplicatedFields<Health>: sizeof=" << sizeof(wire) << " (native " << sizeof(Health)
			  << "), current=" << wire.current << "\n";

	PerEnumerator<EventKind, std::size_t>::Storage counters{};
	counters.Damage = 42;
	std::cout << "  PerEnumerator<EventKind>: Damage=" << counters.Damage << ", Heal=" << counters.Heal << "\n";
}
