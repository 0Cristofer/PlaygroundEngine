export module PlaygroundEngine.Reflection.Core:FunctionInfo;

import :TypedRef;
import :TypeReference;
import :DeclarationInfo;
import :ParameterInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeMetaOf();

	export struct InvokeError
	{
		enum Kind : std::uint8_t
		{
			ArityMismatch,
			TypeMismatch,
			NullArgument,
			ConstViolation,
			NotMovable,
			ReturnTypeMismatch,
			NotInvocable,
		};

		Kind Reason;
		std::uint16_t ArgumentIndex = 0;
	};

	export using Invoker = std::expected<void, InvokeError> (*)(void* obj, std::span<const TypedRef> args, const TypedRef& ret);

	export template <typename Return>
	using InvokeResult = std::conditional_t<std::is_reference_v<Return>, std::reference_wrapper<std::remove_reference_t<Return>>, Return>;

	export enum class RefQualifier : std::uint8_t
	{
		None,
		LValue,
		RValue,
	};

	export struct FunctionTraits
	{
		// The language facts of one member function. IsStatic and IsConst are separate facts: conflating them
		// is correct for the invoke path (a static ignores the object) but a lie as metadata, since a C#
		// generator projects a static differently from a const. IsConstCallable() derives the invoke rule.

		AccessKind Access = AccessKind::Public;

		bool IsStatic = false;

		// A namespace-scope function, which is not a class member at all. Distinct from IsStatic: a static
		// member is scoped to its class, and a C# or scripting projection places the two differently.
		bool IsFreeFunction = false;

		bool IsConst = false;
		bool IsNoexcept = false;

		bool IsVirtual = false;
		bool IsPureVirtual = false;
		bool IsOverride = false;
		bool IsDeleted = false;

		// A compiler-generated special member (a defaulted operator==, the implicit copy assignment). It tells a
		// C# generator or serializer that the behaviour is the language's default, not hand-written.
		bool IsDefaulted = false;

		// A consteval (immediate) function reflects with no invoker: it cannot be called from a runtime thunk.
		// Storing it turns that silent absence into a stated reason, the way IsDeleted does.
		bool IsConsteval = false;

		// A deducing-this member: its object parameter is dropped from GetParams (so the reflected arity is
		// the caller's), and IsConst / RefQual are read off that parameter rather than the function. Stated
		// so a consumer projecting the object as an argument (a C# generator) does not miscount it.
		bool HasExplicitObjectParameter = false;

		// An rvalue-reference-qualified function reflects with no invoker; storing the qualifier turns that
		// silent absence into a stated reason.
		RefQualifier RefQual = RefQualifier::None;

		// The return type is stored decayed, so these carry what decaying it loses: a function returning
		// const std::string& is InvokeAs-able as one, and only this says so.
		bool ReturnIsConst = false;
		bool ReturnIsLvalueReference = false;
		bool ReturnIsRvalueReference = false;
	};

	export class FunctionInfo : public DeclarationInfo
	{
	public:
		constexpr FunctionInfo(const TypeReference returnType,
							   const std::string_view identifier,
							   const std::string_view displayName,
							   const std::span<const std::string_view> scopePath,
							   const std::span<const ParameterInfo> params,
							   const FunctionTraits& traits,
							   const Invoker invoke,
							   const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _returnType(returnType), _params(params), _traits(traits),
			  _invoke(invoke)
		{}

		const TypeInfo& GetReturnType() const;
		std::span<const ParameterInfo> GetParams() const;

		const FunctionTraits& GetTraits() const
		{
			return _traits;
		}
		bool IsStatic() const
		{
			return _traits.IsStatic;
		}
		bool IsFreeFunction() const
		{
			return _traits.IsFreeFunction;
		}
		bool IsConst() const
		{
			return _traits.IsConst;
		}
		bool IsNoexcept() const
		{
			return _traits.IsNoexcept;
		}
		bool IsVirtual() const
		{
			return _traits.IsVirtual;
		}
		bool IsPureVirtual() const
		{
			return _traits.IsPureVirtual;
		}
		bool IsOverride() const
		{
			return _traits.IsOverride;
		}
		bool IsDeleted() const
		{
			return _traits.IsDeleted;
		}
		bool IsDefaulted() const
		{
			return _traits.IsDefaulted;
		}
		bool IsConsteval() const
		{
			return _traits.IsConsteval;
		}
		bool HasExplicitObjectParameter() const
		{
			return _traits.HasExplicitObjectParameter;
		}
		RefQualifier GetRefQualifier() const
		{
			return _traits.RefQual;
		}
		AccessKind GetAccess() const
		{
			return _traits.Access;
		}

		// Callable on a const object: a static and a free function both ignore the object pointer entirely, so
		// they qualify without being const. Derived rather than stored, so the language facts stay separate.
		bool IsConstCallable() const
		{
			return _traits.IsConst || _traits.IsStatic || _traits.IsFreeFunction;
		}

		std::expected<void, InvokeError> Invoke(void* obj, std::span<const TypedRef> args, const TypedRef& ret = {}) const;
		std::expected<void, InvokeError> Invoke(const void* obj, std::span<const TypedRef> args, const TypedRef& ret = {}) const;

		template <typename Return = void, typename Object, typename... Arguments>
		std::expected<InvokeResult<Return>, InvokeError> InvokeAs(Object* obj, Arguments&&... arguments) const
		{
			const auto args = detail::MakeTypedRefs(std::forward<Arguments>(arguments)...);

			if constexpr (std::is_void_v<Return>)
			{
				return Invoke(obj, args);
			}
			else if constexpr (std::is_reference_v<Return>)
			{
				using Referent = std::remove_reference_t<Return>;
				Referent* pointer = nullptr;
				const auto result = Invoke(obj, args, TypedRef{.Type = &TypeMetaOf<Referent*>(), .Data = &pointer, .IsConst = false});
				if (!result)
				{
					return std::unexpected(result.error());
				}

				return std::reference_wrapper<Referent>(*pointer);
			}
			else
			{
				return detail::ValueFromSlot<Return, InvokeError>(
					[this, obj](const std::span<const TypedRef> callArgs, const TypedRef& ret) { return Invoke(obj, callArgs, ret); }, args);
			}
		}

	private:
		TypeReference _returnType;
		std::span<const ParameterInfo> _params;
		FunctionTraits _traits;

		// Null until the type is named through TypeOf<T> (the demand upgrade fills it in place), and null after
		// that for a member the thunk cannot call. SetInvoker is the only writer.
		Invoker _invoke = nullptr;

		// The demand upgrade sets the invoker in place, once, at static init. A hidden friend rather than a
		// public setter, so the invoker stays write-once-by-the-builder (reached only through ADL), never
		// caller-mutable. OperatorInfo and ConversionInfo derive from FunctionInfo, so ADL finds this for them.
		friend void SetInvoker(FunctionInfo& function, const Invoker invoke)
		{
			function._invoke = invoke;
		}
	};
}
