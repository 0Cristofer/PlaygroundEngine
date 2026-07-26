#pragma once

// Typed per-field operations and property matching: the payoff of member_injection.h. define_aggregate
// cannot inject a function, but an injected member's TYPE can be a template carrying behaviour, bound to
// one field by its reflection. See docs/ReflectionInjection.md.

struct Player { int health; float speed; bool isAlive; };

// Behaviour written once, specialised per field. std::meta::info is structural, so it can be a non-type
// template parameter, and the accessor splices the member internally.
template <std::meta::info MetaField>
struct FieldAccessor
{
	using Owner = [:std::meta::parent_of(MetaField):];
	using Type = [:std::meta::type_of(MetaField):];

	static constexpr std::string_view Name = std::meta::identifier_of(MetaField);
	static constexpr int ByteOffset = static_cast<int>(std::meta::offset_of(MetaField).bytes);

	static constexpr const Type& Get(const Owner& owner) { return owner.[:MetaField:]; }
	static constexpr void Set(Owner& owner, const Type& value) { owner.[:MetaField:] = value; }

	static std::string Serialize(const Owner& owner) { return std::format("{}={}", Name, owner.[:MetaField:]); }
};

// no_unique_address collapses the stateless accessors, so the whole table costs one byte.
template <typename T>
class Accessors
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
				std::meta::substitute(^^FieldAccessor, {std::meta::reflect_constant(member)}),
				{.name = std::meta::identifier_of(member), .no_unique_address = true}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}

	static constexpr Storage Of{};
};

// One constexpr index per field, pointing into the canonical field list rather than duplicating it.
// Copying FieldInfo into the injected members instead would silently break identity comparison against
// the list the reflection system hands out.
template <typename T>
class FieldIndex
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
				std::meta::data_member_spec(^^std::size_t, {.name = std::meta::identifier_of(member)}));
		}

		std::meta::define_aggregate(^^Storage, memberSpecs);
	}

private:
	template <std::size_t... MemberIndices>
	static consteval Storage BuildStorage(std::index_sequence<MemberIndices...>)
	{
		return Storage{MemberIndices...};
	}

public:
	static constexpr std::size_t FieldCount =
		std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
			.size();

	static constexpr Storage Of = BuildStorage(std::make_index_sequence<FieldCount>{});
};

// The same matching without any injection: the field names itself by its own reflection. Equally
// compile-time checked, works on overloads and private members, and has no name-uniqueness ceiling. What
// injection adds over this is autocomplete, not safety.
template <std::meta::info MetaField>
consteval std::size_t FieldIndexOf()
{
	constexpr auto members = std::define_static_array(
		std::meta::nonstatic_data_members_of(std::meta::parent_of(MetaField), std::meta::access_context::unchecked()));

	std::size_t index = 0;
	for (const std::meta::info member : members)
	{
		if (member == MetaField)
		{
			return index;
		}

		++index;
	}

	return members.size();
}

// UE's PostEditChangeProperty, minus UHT, minus GET_MEMBER_NAME_CHECKED, and minus the FName compare.
// A constexpr index is a usable case label, so property dispatch becomes a jump table instead of the
// if-else chain of string comparisons UE is limited to.
void OnPropertyChanged(std::size_t changedFieldIndex)
{
	if (changedFieldIndex == FieldIndex<Player>::Of.health)
	{
		std::cout << "    health changed -> clamp against maximum\n";
	}

	switch (changedFieldIndex)
	{
		case FieldIndexOf<^^Player::speed>():
			std::cout << "    speed changed -> rebuild movement (matched without injection)\n";
			break;

		case FieldIndexOf<^^Player::isAlive>():
			std::cout << "    isAlive changed -> toggle ragdoll (matched without injection)\n";
			break;

		default:
			break;
	}
}

struct Mixed
{
	int healthy;

	// Accessors are generated for this field too, yet the build stays clean under -Werror because
	// nothing calls them. Touching it would fail at the CALL SITE with the deprecation named, which is
	// finer-grained demand gating than materialising a whole type's operations at once.
	[[deprecated("use healthy")]] int legacyField;
};

// -----------------------------------------------------------------------------
// Demo
// -----------------------------------------------------------------------------
void DemoMemberAccessors()
{
	std::cout << "\n=== Member Accessors ===\n";

	Player player{.health = 50, .speed = 3.5f, .isAlive = true};

	Accessors<Player>::Of.health.Set(player, 75);
	std::cout << "  typed access: health=" << Accessors<Player>::Of.health.Get(player)
			  << ", speed=" << Accessors<Player>::Of.speed.Get(player) << "\n";

	// Get returns the field's own type, not an erased handle, so there is nothing to unbox or check.
	static_assert(std::is_same_v<decltype(Accessors<Player>::Of.health.Get(player)), const int&>);
	static_assert(std::is_same_v<decltype(Accessors<Player>::Of.speed.Get(player)), const float&>);

	std::cout << "  one pattern, every field: " << Accessors<Player>::Of.health.Serialize(player) << " | "
			  << Accessors<Player>::Of.speed.Serialize(player) << " | "
			  << Accessors<Player>::Of.isAlive.Serialize(player) << "\n";
	std::cout << "  sizeof(Accessors<Player>::Storage)=" << sizeof(Accessors<Player>::Storage)
			  << " for 3 accessors\n";

	static_assert(FieldIndex<Player>::Of.isAlive == 2);
	static_assert(FieldIndexOf<^^Player::isAlive>() == 2);

	for (std::size_t fieldIndex = 0; fieldIndex < FieldIndex<Player>::FieldCount; ++fieldIndex)
	{
		std::cout << "  property " << fieldIndex << " changed:\n";
		OnPropertyChanged(fieldIndex);
	}

	const Mixed mixed{};
	std::cout << "  demand gating: healthy=" << Accessors<Mixed>::Of.healthy.Get(mixed)
			  << ", accessors for the deprecated field exist but were never instantiated\n";
}
