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
		// Excludes constructors, destructors, operators, conversion functions, and other special members. The
		// operators and conversions are modelled in their own lists (GetOperators / GetConversions), where a
		// consumer keys on the operator kind or the target type rather than on a name a conversion does not have.
		std::vector<std::meta::info> functions;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_function(member) && !std::meta::is_special_member_function(member) && !std::meta::is_constructor(member) &&
				!std::meta::is_destructor(member) && !std::meta::is_operator_function(member) && !std::meta::is_conversion_function(member))
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

	consteval bool CallsWithoutObject(const std::meta::info function)
	{
		// A static member and a namespace-scope function are both called with no object. is_static_member is
		// false for a free function, so it cannot carry this on its own.
		return std::meta::is_static_member(function) || !std::meta::is_class_member(function);
	}

	template <typename T, std::meta::info MetaFunction, std::size_t... I>
	decltype(auto) DoCall([[maybe_unused]] void* obj, [[maybe_unused]] std::span<const TypedRef> args, std::index_sequence<I...>)
	{
		// Bind arguments inline at the call so a by-value parameter is initialized directly from the bound
		// prvalue (guaranteed copy elision): no extra move, and a deleted-move type still binds by copy.
		// Parameters exclude a deducing-this object parameter, which the object binds instead.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(CallParametersOf(MetaFunction));

		// An address is taken only where access control demands it, because some functions have none: an
		// always_inline member of an extern-templated class (std::allocator<char>::allocate) is emitted
		// nowhere. See docs/ReflectionInternals.md (function invocation).
		if constexpr (std::meta::is_public(MetaFunction))
		{
			if constexpr (CallsWithoutObject(MetaFunction))
			{
				return [:MetaFunction:](ArgumentBinding<parameters[I]>::Bind(args[I])...);
			}
			else
			{
				// The member-splice call binds the object implicitly, whether it is an implicit this or a
				// deducing-this object parameter, so both take the same route here.
				return (static_cast<T*>(obj)->[:MetaFunction:])(ArgumentBinding<parameters[I]>::Bind(args[I])...);
			}
		}
		else
		{
			// GCC applies access control to a spliced call but not to a call through a pointer, so the
			// pointer is what keeps private-member metadata invocable and symmetric with private fields.
			constexpr auto pointer = &[:MetaFunction:];
			if constexpr (CallsWithoutObject(MetaFunction))
			{
				return pointer(ArgumentBinding<parameters[I]>::Bind(args[I])...);
			}
			else if constexpr (HasExplicitObjectParameter(MetaFunction))
			{
				// &f of a deducing-this member is a plain function pointer taking the object first, not a
				// pointer-to-member, so it is called free-function style with the object as first argument.
				return pointer(*static_cast<T*>(obj), ArgumentBinding<parameters[I]>::Bind(args[I])...);
			}
			else
			{
				return (static_cast<T*>(obj)->*pointer)(ArgumentBinding<parameters[I]>::Bind(args[I])...);
			}
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
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(CallParametersOf(MetaFunction));

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
		constexpr std::size_t parameterCount = CallParametersOf(MetaFunction).size();
		if (args.size() != parameterCount)
		{
			return std::unexpected(InvokeError{.Reason = InvokeError::ArityMismatch, .ArgumentIndex = 0});
		}

		return InvokeThunkImpl<T, MetaFunction>(obj, args, ret, std::make_index_sequence<parameterCount>{});
	}

	template <typename T, std::meta::info MetaFunction, std::size_t... I>
	consteval bool IsInvocableImpl(std::index_sequence<I...>)
	{
		// Mirror DoCall exactly (same call route, same ArgumentBinding::Bind per argument). A function the
		// thunk cannot call on the erased lvalue (an rvalue-ref-qualified overload) reflects as metadata with
		// no invoker, the way a bitfield reflects with no borrow, rather than failing the whole type's build.
		[[maybe_unused]] constexpr auto parameters = std::define_static_array(CallParametersOf(MetaFunction));
		if constexpr (std::meta::is_public(MetaFunction))
		{
			if constexpr (CallsWithoutObject(MetaFunction))
			{
				return requires { [:MetaFunction:](ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
			}
			else
			{
				return requires(T* obj) { (obj->[:MetaFunction:])(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
			}
		}
		else if constexpr (CallsWithoutObject(MetaFunction))
		{
			return requires { (&[:MetaFunction:])(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
		}
		else if constexpr (HasExplicitObjectParameter(MetaFunction))
		{
			return requires(T* obj) { (&[:MetaFunction:])(*obj, ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
		}
		else
		{
			return requires(T* obj) { (obj->*(&[:MetaFunction:]))(ArgumentBinding<parameters[I]>::Bind(std::declval<const TypedRef&>())...); };
		}
	}

	template <typename T, std::meta::info MetaFunction>
	consteval bool IsInvocable()
	{
		// Deleted and consteval functions are never invocable, excluded by the stated fact rather than by
		// SFINAE: naming a deleted one in the requires-expression is a hard error, and a consteval one is
		// accepted there but then called at runtime by the thunk, which is a hard error too (not a null invoker).
		if constexpr (std::meta::is_deleted(MetaFunction) || IsImmediateFunction(MetaFunction))
		{
			return false;
		}
		else
		{
			return IsInvocableImpl<T, MetaFunction>(std::make_index_sequence<CallParametersOf(MetaFunction).size()>{});
		}
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

	consteval RefQualifier RefQualifierOf(const std::meta::info function)
	{
		if (std::meta::is_lvalue_reference_qualified(function))
		{
			return RefQualifier::LValue;
		}
		if (std::meta::is_rvalue_reference_qualified(function))
		{
			return RefQualifier::RValue;
		}
		return RefQualifier::None;
	}

	consteval RefQualifier ObjectRefQualifierOf(const std::meta::info objectType)
	{
		if (std::meta::is_rvalue_reference_type(objectType))
		{
			return RefQualifier::RValue;
		}
		if (std::meta::is_lvalue_reference_type(objectType))
		{
			return RefQualifier::LValue;
		}
		return RefQualifier::None;
	}

	consteval bool ObjectIsConstCallable(const std::meta::info objectType)
	{
		// A deducing-this member binds without a mutable object, so it is const-callable, when its object is a
		// const reference or taken by value (a copy leaves the caller's object untouched). A mutable lvalue
		// reference or an rvalue reference is not, so IsConst reports the same as an ordinary const member.
		return std::meta::is_const_type(std::meta::remove_reference(objectType)) || !std::meta::is_reference_type(objectType);
	}

	template <std::meta::info MetaFunction>
	consteval FunctionTraits MakeFunctionTraits()
	{
		constexpr std::meta::info returnType = std::meta::return_type_of(MetaFunction);

		// A deducing-this member carries its const-ness and ref-qualification on the object parameter, not on
		// the function, so those two facts are read off the object type; every other function reports them
		// on itself. is_static is always false for a deducing-this member, so IsStatic needs no such split.
		constexpr bool hasExplicitObject = HasExplicitObjectParameter(MetaFunction);
		constexpr bool isConst = hasExplicitObject ? ObjectIsConstCallable(std::meta::type_of(std::meta::parameters_of(MetaFunction).front()))
												   : std::meta::is_const(MetaFunction);
		constexpr RefQualifier refQual = hasExplicitObject ? ObjectRefQualifierOf(std::meta::type_of(std::meta::parameters_of(MetaFunction).front()))
														   : RefQualifierOf(MetaFunction);

		return FunctionTraits{
			.Access = AccessOf(MetaFunction),
			.IsStatic = std::meta::is_static_member(MetaFunction),
			.IsFreeFunction = !std::meta::is_class_member(MetaFunction),
			.IsConst = isConst,
			.IsNoexcept = std::meta::is_noexcept(MetaFunction),
			.IsVirtual = std::meta::is_virtual(MetaFunction),
			.IsPureVirtual = std::meta::is_pure_virtual(MetaFunction),
			.IsOverride = std::meta::is_override(MetaFunction),
			.IsDeleted = std::meta::is_deleted(MetaFunction),
			.IsDefaulted = std::meta::is_defaulted(MetaFunction),
			.IsConsteval = IsImmediateFunction(MetaFunction),
			.HasExplicitObjectParameter = hasExplicitObject,
			.RefQual = refQual,
			.ReturnIsConst = std::meta::is_const_type(std::meta::remove_reference(returnType)),
			.ReturnIsLvalueReference = std::meta::is_lvalue_reference_type(returnType),
			.ReturnIsRvalueReference = std::meta::is_rvalue_reference_type(returnType),
		};
	}

	template <std::meta::info MetaType, std::meta::info MetaFunction>
	consteval FunctionInfo MakeFunction()
	{
		using T = [:MetaType:];

		return FunctionInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::return_type_of(MetaFunction))>(), IdentifierOf(MetaFunction),
							DisplayStringOf(MetaFunction), ScopePathOf<MetaFunction>(), MakeParameters<MetaFunction>(),
							MakeFunctionTraits<MetaFunction>(), MakeInvoker<T, MetaFunction>(), MakeAnnotations<MetaFunction>());
	}

	export template <std::meta::info MetaFunction>
	constexpr const FunctionInfo& FunctionOfMeta()
	{
		// A member here would build a second FunctionInfo for an entity its type already owns, and the helper
		// instantiations collide at link as a bare duplicate-symbol error naming no source line.
		static_assert(!std::meta::is_class_member(MetaFunction),
					  "FunctionOfMeta reflects namespace-scope functions; reach a member through TypeOf<T>().GetFunctions()");

		// A namespace-scope function has no owning type, so the object type is void: every branch that would
		// use it is discarded, because CallsWithoutObject is true. One instance per function, so a consumer
		// can compare by pointer identity the way it does for types.
		static constexpr FunctionInfo Function = MakeFunction<^^void, MetaFunction>();
		return Function;
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
