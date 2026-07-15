module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:FunctionsBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :ArgumentBinding;
import :TypeInfo;
import :FunctionInfo;
import :ParameterInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the FunctionInfo list for a type: parameter metadata, the argument-binding and validation
// rules (const/move/arity/return-type erasure), and the runtime invoke thunk that drives the call.

namespace PgE::detail
{
	consteval std::vector<std::meta::info> GetMemberFunctions(const std::meta::info type)
	{
		// Excludes constructors, destructors, operators, and other special members.
		std::vector<std::meta::info> functions;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_function(member) && !std::meta::is_special_member_function(member) && !std::meta::is_constructor(member) &&
				!std::meta::is_destructor(member) && !std::meta::is_operator_function(member))
			{
				functions.push_back(member);
			}
		}
		return functions;
	}

	constexpr InvokeError::Kind ToInvokeError(const ArgumentError error)
	{
		// None is not a failure and never reaches here, since CheckInvokeArgument returns on it first. It
		// cannot be a precondition: GCC 16 leaves __tu_has_violation undefined for a contract on an inline
		// free function in a .cppm. See docs/ReflectionInternals.md (the argument binder).
		switch (error)
		{
		case ArgumentError::NullArgument:
			return InvokeError::NullArgument;
		case ArgumentError::ConstViolation:
			return InvokeError::ConstViolation;
		case ArgumentError::NotMovable:
			return InvokeError::NotMovable;
		case ArgumentError::TypeMismatch:
		case ArgumentError::None:
			break;
		}

		return InvokeError::TypeMismatch;
	}

	template <std::meta::info MetaParameter>
	void CheckInvokeArgument(const TypedRef& arg, const std::size_t index, bool& valid, InvokeError& error)
	{
		// Map the shared neutral check onto InvokeError. A free function (not a lambda) keeps the spliced
		// parameter reflection in a constant-evaluated context (GCC -Wtemplate-body).
		if (!valid)
		{
			return;
		}

		const ArgumentError result = CheckArgument<MetaParameter>(arg);
		if (result == ArgumentError::None)
		{
			return;
		}

		valid = false;
		error = {.Reason = ToInvokeError(result), .ArgumentIndex = static_cast<std::uint16_t>(index)};
	}

	template <typename T, std::meta::info MetaFunction, std::size_t... I>
	decltype(auto) DoCall([[maybe_unused]] void* obj, [[maybe_unused]] std::span<const TypedRef> args, std::index_sequence<I...>)
	{
		// Bind arguments inline at the call so a by-value parameter is initialized directly from the
		// bound prvalue (guaranteed copy elision): no extra move, and a type with a deleted move
		// constructor still binds by copy.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaFunction));

		// Call through &[:MetaFunction:], not a direct member splice: it lets reflection invoke private member
		// functions (GCC applies access control to a spliced call but not to a call through a pointer).
		// See docs/ReflectionInternals.md (function invocation through a pointer).
		constexpr auto pointer = &[:MetaFunction:];
		if constexpr (std::meta::is_static_member(MetaFunction))
		{
			return pointer(ArgumentBinding<parameters[I]>::Bind(args[I])...);
		}
		else
		{
			return (static_cast<T*>(obj)->*pointer)(ArgumentBinding<parameters[I]>::Bind(args[I])...);
		}
	}

	template <typename PointerType>
	const TypeInfo* PointerReturnTag()
	{
		return &TypeOfMeta<^^PointerType>();
	}

	template <typename T, std::meta::info MetaFunction, std::size_t... I>
	std::expected<void, InvokeError> InvokeThunkImpl([[maybe_unused]] void* obj,
													 [[maybe_unused]] std::span<const TypedRef> args,
													 [[maybe_unused]] const TypedRef& ret,
													 std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaFunction));

		bool valid = true;
		InvokeError error{};

		(CheckInvokeArgument<parameters[I]>(args[I], I, valid, error), ...);
		if (!valid)
		{
			return std::unexpected(error);
		}

		using Return = [:std::meta::return_type_of(MetaFunction):];
		if constexpr (std::is_void_v<Return>)
		{
			if (ret.Type != nullptr)
			{
				return std::unexpected(InvokeError{.Reason = InvokeError::ReturnTypeMismatch, .ArgumentIndex = 0});
			}

			DoCall<T, MetaFunction>(obj, args, std::index_sequence<I...>{});
		}
		else if constexpr (std::is_reference_v<Return>)
		{
			// A reference return is erased as a pointer to the referent, tagged as that pointer type
			// so it stays distinct from a same-typed value return.
			using Referent = std::remove_reference_t<Return>;
			if (ret.Type != nullptr && ret.Type != PointerReturnTag<Referent*>())
			{
				return std::unexpected(InvokeError{InvokeError::ReturnTypeMismatch, 0});
			}

			Return result = DoCall<T, MetaFunction>(obj, args, std::index_sequence<I...>{});
			if (ret.Type != nullptr)
			{
				*static_cast<Referent**>(ret.Data) = std::addressof(result);
			}
		}
		else
		{
			const TypeInfo* returnTag = &TypeOfMeta<std::meta::remove_cvref(std::meta::return_type_of(MetaFunction))>();
			if (ret.Type != nullptr && ret.Type != returnTag)
			{
				return std::unexpected(InvokeError{.Reason = InvokeError::ReturnTypeMismatch, .ArgumentIndex = 0});
			}

			Return result = DoCall<T, MetaFunction>(obj, args, std::index_sequence<I...>{});
			if (ret.Type != nullptr)
			{
				std::construct_at(static_cast<Return*>(ret.Data), std::move(result));
			}
		}

		return {};
	}

	template <typename T, std::meta::info MetaFunction>
	std::expected<void, InvokeError> InvokeThunk(void* obj, std::span<const TypedRef> args, const TypedRef& ret)
	{
		constexpr std::size_t parameterCount = std::meta::parameters_of(MetaFunction).size();
		if (args.size() != parameterCount)
		{
			return std::unexpected(InvokeError{.Reason = InvokeError::ArityMismatch, .ArgumentIndex = 0});
		}

		return InvokeThunkImpl<T, MetaFunction>(obj, args, ret, std::make_index_sequence<parameterCount>{});
	}

	template <typename T, std::meta::info MetaFunction, std::size_t... I>
	consteval bool IsInvocableImpl(std::index_sequence<I...>)
	{
		// Mirror DoCall exactly (same pointer call, same ArgumentBinding::Bind per argument). A function the
		// thunk cannot call on the erased lvalue (an rvalue-ref-qualified overload) reflects as metadata with
		// no invoker, the way a bitfield reflects with no borrow, rather than failing the whole type's build.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(std::meta::parameters_of(MetaFunction));
		if constexpr (std::meta::is_static_member(MetaFunction))
		{
			return requires { (&[:MetaFunction:])(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
		}
		else
		{
			return requires(T* obj) { (obj->*(&[:MetaFunction:]))(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
		}
	}

	template <typename T, std::meta::info MetaFunction>
	consteval bool IsInvocable()
	{
		return IsInvocableImpl<T, MetaFunction>(std::make_index_sequence<std::meta::parameters_of(MetaFunction).size()>{});
	}

	template <typename T, std::meta::info MetaFunction>
	consteval Invoker MakeInvoker()
	{
		if constexpr (IsInvocable<T, MetaFunction>())
		{
			return &InvokeThunk<T, MetaFunction>;
		}
		else
		{
			return nullptr;
		}
	}

	template <std::meta::info MetaType, std::meta::info MetaFunction>
	consteval FunctionInfo MakeFunction()
	{
		using T = [:MetaType:];
		const bool constCallable = std::meta::is_const(MetaFunction) || std::meta::is_static_member(MetaFunction);

		return FunctionInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::return_type_of(MetaFunction))>(), IdentifierOf(MetaFunction),
							DisplayStringOf(MetaFunction), MakeParameters<MetaFunction>(), constCallable, MakeInvoker<T, MetaFunction>(),
							MakeAnnotations<MetaFunction>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeFunctionArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto functions = std::define_static_array(GetMemberFunctions(MetaType));
		return std::array<FunctionInfo, sizeof...(I)>{MakeFunction<MetaType, functions[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeFunctionsFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<FunctionInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto functionCount = std::define_static_array(GetMemberFunctions(MetaType)).size();
			return MakeFunctionArray<MetaType>(std::make_index_sequence<functionCount>{});
		}
		else
		{
			return std::array<FunctionInfo, 0>{};
		}
	}
}
