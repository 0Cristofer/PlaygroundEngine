#pragma once

// =============================================================================
// Field Printing
//
// The simplest reflection use-case: enumerate every data field of a struct and
// print its name and current value — with zero per-type boilerplate.
//
// Concepts demonstrated:
//   ^^T                          — reflects type T into a std::meta::info handle
//   std::meta::identifier_of(r)  — extracts a name from a reflection as string_view
//   nonstatic_data_members_of(r) — returns the compile-time list of data members
//   std::define_static_array(r)  — materialises a reflection range into a static span
//   template for (constexpr …)   — compile-time loop (each iteration is its own instantiation)
//   obj.[:member:]               — splicer: turns a member reflection back into a field access
// =============================================================================

struct Vec3 { float x, y, z; };

// Prints every non-static data member of T alongside its value.
// Works for any struct without any per-type registration.
template <typename T>
void PrintMembers(const T& obj)
{
	// ^^T produces a compile-time "reflection" of the type — a std::meta::info value.
	std::cout << "Type: " << std::meta::identifier_of(^^T) << "\n";

	// nonstatic_data_members_of returns a compile-time list of member reflections.
	// define_static_array materialises it into a static std::span we can loop over.
	// access_context::current() respects the access rules at the call site.
	template for (constexpr auto member :
		std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())))
	{
		// obj.[:member:] is a "splicer": given a compile-time member reflection,
		// it generates the actual field access expression (e.g. obj.x, obj.y).
		std::cout << "  " << std::meta::identifier_of(member) << " = " << obj.[:member:] << "\n";
	}
}

void DemoFieldPrinting()
{
	std::cout << "=== Field Printing ===\n";
	PrintMembers(Vec3{1.f, 2.f, 3.f});
	PrintMembers(Vec3{4.f, 5.f, 6.f});
}
