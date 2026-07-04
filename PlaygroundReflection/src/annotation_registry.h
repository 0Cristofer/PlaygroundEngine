#pragma once

// =============================================================================
// Annotation Filtering + Type-Specific Function Registry
//
// Shows how to attach compile-time tags to fields and query them at reflection
// time, then builds a simple name→function-pointer map for a concrete type.
// This is the "simple" approach with a fixed signature; see generic_registry.h
// for the general case that works across any type and any signature.
//
// Concepts demonstrated:
//   [[=Tag{}]]                       : attaches an empty tag as a compile-time annotation
//   [[=Range{0, 100}]]               : attaches a VALUE-carrying annotation
//   std::meta::annotations_of_with_type(member, ^^Tag)
//                                    : retrieves annotations of a specific type on a member
//   std::meta::extract<Tag>(anno)    : reads the stored value back out of an annotation
//   +[](…){ … }                      : converts a captureless lambda to a plain function pointer
//   self.[:member:]()                : splicer used to call a reflected member function
// =============================================================================

// Empty tag struct. Attaching it as [[=Replicated{}]] on a field marks it
// for network replication. Any empty struct can serve as an annotation tag.
struct Replicated {};

struct PlayerState
{
	// [[=Replicated{}]] attaches a Replicated instance as a compile-time annotation.
	// Fields without the annotation are invisible to the replication system.
	[[=Replicated{}]] int health          = 100;
					  int internalCounter = 0;

	int Damage(int amount) { health -= amount; return health; }
	int Heal  (int amount) { health += amount; return health; }
};

// Builds a name → function-pointer map for PlayerState specifically.
// Every thunk has the same fixed signature (PlayerState&, int) → int,
// which keeps the code simple but limits it to this one type and signature.
using PlayerStateThunk = int (*)(PlayerState&, int);

std::unordered_map<std::string_view, PlayerStateThunk> BuildPlayerStateRegistry()
{
	std::unordered_map<std::string_view, PlayerStateThunk> registry;

	// members_of returns all members including functions.
	// access_context::unchecked() lets us see all members regardless of access specifier.
	template for (constexpr auto member :
		std::define_static_array(std::meta::members_of(^^PlayerState, std::meta::access_context::unchecked())))
	{
		// Skip constructors, destructors, copy/move operators; only keep user-defined functions.
		if constexpr (std::meta::is_function(member) && !std::meta::is_special_member_function(member))
		{
			// The unary + forces the lambda to decay to a plain function pointer.
			// self.[:member:](arg) splices the reflected function into a real member call.
			registry[std::meta::identifier_of(member)] =
				+[](PlayerState& self, int arg) { return self.[:member:](arg); };
		}
	}
	return registry;
}

// =============================================================================
// Value-carrying annotations
//
// An annotation is not limited to a presence flag: the attached object can hold
// data, read back at compile time with std::meta::extract<T>. This is the
// substrate for editor property metadata (use case 5): sliders, tooltips,
// grouping, declared inline on the field instead of in a side table.
//
// Two constraints the toolchain enforces here:
//   1. The annotation type must be STRUCTURAL (a valid non-type template
//      argument): public members, no private state. std::string_view fails
//      (its pointer/length members are private), and a raw const char* to a
//      string literal fails too: a string literal has no guaranteed unique
//      address, so it is not a valid constant and extract<T> rejects it. The
//      fix is std::define_static_string, which promotes the literal to a
//      uniqued static array and hands back a stably-addressed pointer.
//   2. An annotation cannot be spliced ([:anno:] is rejected). extract<T> is
//      the only way to recover the value.
// =============================================================================

// A string annotation value. define_static_string does the work: it interns the
// literal into a static array, so the stored pointer is a self-contained
// constant with static lifetime. That means no fixed capacity, no copy, and the
// View() stays valid even after the extracted constant goes out of scope. The
// const char* parameter keeps nested init (Category{"Combat"}) a single
// user-defined conversion.
struct AnnotationString
{
	const char* text;

	consteval AnnotationString(const char* literal)
		: text(std::define_static_string(std::string_view{literal})) {}

	constexpr std::string_view View() const { return text; }
};

// Editor-facing annotations. Range drives a slider, Category groups fields in
// the inspector, Tooltip is the hover text.
struct Range    { double min; double max; };
struct Category { AnnotationString name; };
struct Tooltip  { AnnotationString text; };

// A struct authored with editor metadata attached directly to its fields.
// A field may carry several annotations of different types at once.
struct WeaponStats
{
	[[=Category{"Combat"}]] [[=Range{0.0, 500.0}]] [[=Tooltip{"Damage dealt per hit"}]]
	float damage = 42.f;

	[[=Category{"Combat"}]] [[=Range{0.0, 10.0}]]
	float attacksPerSecond = 1.5f;

	[[=Category{"Economy"}]] [[=Tooltip{"Gold cost in the shop"}]]
	int cost = 300;

	// No annotations: still a real field, just invisible to the editor metadata.
	int internalId = 0;
};

// The runtime shape a real FieldInfo would expose: the compile-time annotation
// values baked into plain data the editor can consume without any reflection.
struct PropertyMetadata
{
	std::string_view name{};
	std::string_view category{};
	std::string_view tooltip{};
	bool             hasRange = false;
	double           rangeMin = 0.0;
	double           rangeMax = 0.0;
};

// Walks T's fields and lifts their annotation values into runtime PropertyMetadata.
// This is the compile-time→runtime bridge: extract<T> pulls each value as a
// constant, and the results are pushed into a runtime vector.
template <typename T>
std::vector<PropertyMetadata> BuildEditorMetadata()
{
	std::vector<PropertyMetadata> properties;

	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
	{
		PropertyMetadata metadata{ .name = std::meta::identifier_of(member) };

		if constexpr (!std::meta::annotations_of_with_type(member, ^^Category).empty())
		{
			constexpr Category category = std::meta::extract<Category>(
				std::meta::annotations_of_with_type(member, ^^Category).front());
			metadata.category = category.name.View();
		}

		if constexpr (!std::meta::annotations_of_with_type(member, ^^Tooltip).empty())
		{
			constexpr Tooltip tooltip = std::meta::extract<Tooltip>(
				std::meta::annotations_of_with_type(member, ^^Tooltip).front());
			metadata.tooltip = tooltip.text.View();
		}

		if constexpr (!std::meta::annotations_of_with_type(member, ^^Range).empty())
		{
			constexpr Range range = std::meta::extract<Range>(
				std::meta::annotations_of_with_type(member, ^^Range).front());
			metadata.hasRange = true;
			metadata.rangeMin = range.min;
			metadata.rangeMax = range.max;
		}

		properties.push_back(metadata);
	}

	return properties;
}

void DemoAnnotations()
{
	std::cout << "\n=== Annotations ===\n";

	// Walk every data field of PlayerState and check for the Replicated annotation.
	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^PlayerState, std::meta::access_context::unchecked())))
	{
		// annotations_of_with_type returns all annotations of the given type on this member.
		// If the list is non-empty, the annotation is present.
		constexpr bool isReplicated = !std::meta::annotations_of_with_type(member, ^^Replicated).empty();

		std::cout << "  " << std::meta::identifier_of(member)
				  << "  replicated: " << std::boolalpha << isReplicated << "\n";
	}

	std::cout << "\n--- Value-carrying annotations (editor metadata) ---\n";

	// Every value printed here was declared inline on a WeaponStats field and
	// read back out with extract<T>, no side table, no code generator.
	for (const PropertyMetadata& property : BuildEditorMetadata<WeaponStats>())
	{
		std::cout << "  " << property.name;
		if (!property.category.empty()) std::cout << "  [" << property.category << "]";
		if (property.hasRange)          std::cout << "  range(" << property.rangeMin << ", " << property.rangeMax << ")";
		if (!property.tooltip.empty())  std::cout << "  \"" << property.tooltip << "\"";
		std::cout << "\n";
	}

	std::cout << "\n--- Type-specific function registry ---\n";
	PlayerState player;
	auto playerRegistry = BuildPlayerStateRegistry();
	std::cout << "  Damage(30) → " << playerRegistry.at("Damage")(player, 30) << "\n";
	std::cout << "  Heal(10)   → " << playerRegistry.at("Heal")  (player, 10) << "\n";
}
