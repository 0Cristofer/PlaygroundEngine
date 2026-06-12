#pragma once

// =============================================================================
// Generic Any-Erased Function Registry
//
// A registry that works for ANY type and ANY member function signature.
// Arguments and return values are erased to std::any so every function fits
// the same runtime Invoker pointer — at the cost of boxing each value.
//
// Concepts demonstrated:
//   std::meta::parameters_of(Fn)    — compile-time list of a function's parameters
//   std::meta::return_type_of(Fn)   — reflection of a function's return type
//   std::meta::type_of(param)       — reflection of a parameter's type
//   std::index_sequence             — used to unpack parameter types as a pack
//
// GCC-specific workarounds needed in this file:
//   ArgCast<Param>    — GCC forbids a bare splicer as a template argument inside a
//                       function body (-Wtemplate-body). Moving the splice into a
//                       struct's using-alias (which is in a valid consteval context)
//                       and calling a plain static function avoids the restriction.
//   InvokeImpl/Invoke — std::make_index_sequence<N> in a lambda body is also
//                       consteval-only in GCC. Splitting into two functions lifts
//                       the index_sequence construction to template-instantiation
//                       time, which is a constant-evaluated context.
// =============================================================================

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

// Owns the type splice for one function parameter.
//
// GCC restriction: a splicer used directly as a template argument inside a
// function body (e.g. std::any_cast<[:type_of(p):]>) triggers -Wtemplate-body.
// Solution: move the splice into a using-alias here at namespace scope, where
// it is in a valid consteval context. The static from() function then uses the
// resulting 'type' alias normally, avoiding any splicer in template-arg position.
template <std::meta::info Param>
struct ArgCast
{
	// [:…:] is a type splicer: turns the compile-time reflection of Param's type
	// back into a real C++ type that can be used in declarations and casts.
	using type = [:std::meta::type_of(Param):];

	static type from(std::any& argument) { return std::any_cast<type>(argument); }
};

// Core thunk — one instantiation per (T, Fn) pair.
//
// The I... pack is the index sequence for the parameter list, injected by Invoke
// so that std::make_index_sequence is constructed outside any lambda body.
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

// Wrapper that computes the parameter count at template-instantiation time
// (a constant-evaluated context) and passes the resulting index_sequence to InvokeImpl.
//
// The split exists because GCC treats std::make_index_sequence<N> inside a lambda
// body as consteval-only. At function-template instantiation time (here), N is a
// proper constant expression and the restriction does not apply.
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
