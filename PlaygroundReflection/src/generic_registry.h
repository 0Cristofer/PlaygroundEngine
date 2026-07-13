#pragma once

// A generic any-erased function registry: erases any type and signature to a shared Invoker via std::any,
// with the two GCC workarounds (ArgCast moves the splice into a using-alias; the InvokeImpl/Invoke split
// lifts make_index_sequence out of a lambda body). See docs/ReflectionInternals.md.

// --- Demo types ---

struct Calculator
{
	float       Add  (float a, float b) { return a + b; }
	std::string Greet(std::string who)  { return "Hi, " + who; }
	void        Clear()                 { std::cout << "  (Calculator::Clear)\n"; }
};

struct Timer
{
	float elapsed = 0.f;
	float limit   = 5.f;

	void  Tick     (float dt) { elapsed += dt; }
	bool  IsExpired() const   { return elapsed >= limit; }
	float Remaining() const   { return limit - elapsed; }
	void  Reset    ()         { elapsed = 0.f; }
};

// --- Type-erased invoker infrastructure ---

// Every reflected function is erased to this uniform runtime signature.
using Args    = std::span<std::any>;
using Invoker = std::any (*)(void* obj, Args args);

// Owns the type splice for one parameter: a splicer used directly as a template argument in a function
// body triggers GCC -Wtemplate-body, so the splice moves into a using-alias here at namespace scope (a
// valid consteval context), which the static from() then uses normally.
template <std::meta::info Param>
struct ArgCast
{
	// [:…:] is a type splicer: turns the compile-time reflection of Param's type
	// back into a real C++ type that can be used in declarations and casts.
	using type = [:std::meta::type_of(Param):];

	static type from(std::any& argument) { return std::any_cast<type>(argument); }
};

// Core thunk, one instantiation per (T, Fn) pair. The I... index-sequence pack is injected by Invoke so
// make_index_sequence is constructed outside any lambda body.
template <typename T, std::meta::info Fn, std::size_t... I>
std::any InvokeImpl(void* obj, Args args, std::index_sequence<I...>)
{
	// Materialise the parameter reflections into a static array we can index with I.
	constexpr auto parameters = std::define_static_array(std::meta::parameters_of(Fn));

	// Splice the return type into a real C++ type alias for the if constexpr check.
	using ReturnType = [:std::meta::return_type_of(Fn):];

	if constexpr (std::is_void_v<ReturnType>)
	{
		// obj->[:Fn:](…) splices the reflected function into a real member-call expression.
		// ArgCast<parameters[I]>::from(args[I]) casts each std::any to the expected type.
		static_cast<T*>(obj)->[:Fn:](ArgCast<parameters[I]>::from(args[I])...);
		return {};   // std::any() represents void
	}
	else
	{
		return static_cast<T*>(obj)->[:Fn:](ArgCast<parameters[I]>::from(args[I])...);
	}
}

// Wrapper that computes the parameter count at template-instantiation time (a constant-evaluated context)
// and passes the index_sequence to InvokeImpl. The split exists because GCC treats make_index_sequence<N>
// inside a lambda body as consteval-only, but N is a proper constant here.
template <typename T, std::meta::info Fn>
std::any Invoke(void* obj, Args args)
{
	constexpr std::size_t parameterCount = std::meta::parameters_of(Fn).size();
	return InvokeImpl<T, Fn>(obj, args, std::make_index_sequence<parameterCount>{});
}

// Builds the name → Invoker map for any type T.
// Skips special member functions (constructors, destructors, copy/move operators).
template <typename T>
std::unordered_map<std::string_view, Invoker> RegisterType()
{
	std::unordered_map<std::string_view, Invoker> registry;

	template for (constexpr auto member :
		std::define_static_array(std::meta::members_of(^^T, std::meta::access_context::unchecked())))
	{
		if constexpr (std::meta::is_function(member) && !std::meta::is_special_member_function(member))
		{
			// &Invoke<T, member> instantiates one Invoke specialisation per function
			// and stores its address as a plain function pointer in the registry.
			registry[std::meta::identifier_of(member)] = &Invoke<T, member>;
		}
	}
	return registry;
}

void DemoGenericRegistry()
{
	std::cout << "\n=== Generic Function Registry ===\n";

	std::vector<std::any> noArgs;

	// --- Calculator: float return, string return, void return ---
	Calculator calculator;
	auto calculatorRegistry = RegisterType<Calculator>();

	std::vector<std::any> addArgs   = {1.0f, 2.0f};
	std::vector<std::any> greetArgs = {std::string{"World"}};

	std::cout << "  Add(1, 2)      = "
			  << std::any_cast<float>(calculatorRegistry.at("Add")(&calculator, addArgs)) << "\n";
	std::cout << "  Greet(\"World\") = "
			  << std::any_cast<std::string>(calculatorRegistry.at("Greet")(&calculator, greetArgs)) << "\n";
	calculatorRegistry.at("Clear")(&calculator, noArgs);

	// --- Timer: const methods, bool return, void+arg, void no-arg ---
	Timer timer;
	auto timerRegistry = RegisterType<Timer>();

	std::vector<std::any> tickArgs = {3.0f};
	timerRegistry.at("Tick")(&timer, tickArgs);
	std::cout << "  after Tick(3):  Remaining = "
			  << std::any_cast<float>(timerRegistry.at("Remaining")(&timer, noArgs)) << "\n";
	std::cout << "                  IsExpired = "
			  << std::any_cast<bool>(timerRegistry.at("IsExpired")(&timer, noArgs)) << "\n";

	tickArgs = {3.0f};
	timerRegistry.at("Tick")(&timer, tickArgs);
	std::cout << "  after Tick(3) again: IsExpired = "
			  << std::any_cast<bool>(timerRegistry.at("IsExpired")(&timer, noArgs)) << "\n";

	timerRegistry.at("Reset")(&timer, noArgs);
	std::cout << "  after Reset:    Remaining = "
			  << std::any_cast<float>(timerRegistry.at("Remaining")(&timer, noArgs)) << "\n";
}
