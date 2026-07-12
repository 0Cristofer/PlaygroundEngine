#pragma once

// Inheritance, field type names, std::type_index, and enum reflection: bases_of + recursion for inherited
// fields, display_string_of for template type names, typeid(typename [:splice:]) for a type_index, and
// enumerators_of for enums. See docs/ReflectionInternals.md (Validated std::meta patterns).

struct Entity
{
	int  id     = 0;
	bool active = true;
};

// Actor inherits Entity. Used to show that nonstatic_data_members_of returns
// only the direct members (name, speed), not the inherited ones (id, active).
struct Actor : Entity
{
	std::string name  = "Actor";
	float       speed = 1.0f;
};

enum class Team { Red, Blue, Green };

// [:enumerator:] is consteval-only in the current GCC implementation.
// Wrapping the splice inside a consteval function lets us retrieve the value
// from a constexpr initialiser in regular (non-consteval) function bodies.
template <std::meta::info Enumerator>
consteval auto EnumValue() { return [:Enumerator:]; }

void DemoTypeExploration()
{
	// ---- Inheritance ----
	std::cout << "\n=== Inheritance ===\n";

	// nonstatic_data_members_of returns DIRECT members only.
	std::cout << "Actor direct members:\n";
	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^Actor, std::meta::access_context::unchecked())))
	{
		std::cout << "  " << std::meta::identifier_of(member) << "\n";
	}

	// bases_of returns a reflection for each base-class specifier.
	// type_of on a base specifier gives the reflection of the base type itself,
	// which we can then pass back into nonstatic_data_members_of.
	std::cout << "Inherited members (via bases_of):\n";
	template for (constexpr auto base :
		std::define_static_array(std::meta::bases_of(^^Actor, std::meta::access_context::unchecked())))
	{
		std::cout << "  from " << std::meta::identifier_of(std::meta::type_of(base)) << ":\n";

		template for (constexpr auto member :
			std::define_static_array(std::meta::nonstatic_data_members_of(std::meta::type_of(base), std::meta::access_context::unchecked())))
		{
			std::cout << "    " << std::meta::identifier_of(member) << "\n";
		}
	}

	// ---- Field type names ----
	std::cout << "\n=== Field Type Names (Actor) ===\n";
	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^Actor, std::meta::access_context::unchecked())))
	{
		// display_string_of handles template specialisations like std::string,
		// where identifier_of would fail (std::string is really basic_string<char,…>).
		std::cout << "  " << std::meta::identifier_of(member)
				  << " : " << std::meta::display_string_of(std::meta::type_of(member)) << "\n";
	}

	// ---- std::type_index from reflection ----
	std::cout << "\n=== std::type_index (PlayerState) ===\n";
	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^PlayerState, std::meta::access_context::unchecked())))
	{
		// 'typename' tells the compiler the splicer produces a type, not a value.
		// Without it GCC reports "expected a reflection of an expression".
		const std::type_info& typeInfo = typeid(typename [:std::meta::type_of(member):]);

		std::cout << "  " << std::meta::identifier_of(member)
				  << " → type_index: " << std::type_index(typeInfo).name() << "\n";
	}

	// ---- Enum reflection ----
	std::cout << "\n=== Enum Reflection (Team) ===\n";
	template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^Team)))
	{
		// EnumValue<enumerator>() is consteval, so its result must be captured in a
		// constexpr local, this puts the call in a constant-evaluated context,
		// satisfying GCC's restriction on consteval-only expressions.
		constexpr auto value = static_cast<int>(EnumValue<enumerator>());

		std::cout << "  " << std::meta::identifier_of(enumerator) << " = " << value << "\n";
	}
}
