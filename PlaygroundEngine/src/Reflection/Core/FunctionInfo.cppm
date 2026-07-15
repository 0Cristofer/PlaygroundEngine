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
	constexpr const TypeInfo& TypeOf();

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

	export class FunctionInfo : public DeclarationInfo
	{
	public:
		constexpr FunctionInfo(const TypeReference returnType,
							   const std::string_view identifier,
							   const std::string_view displayName,
							   const std::span<const ParameterInfo> params,
							   const bool constCallable,
							   const Invoker invoke,
							   const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, annotations), _returnType(returnType), _params(params), _constCallable(constCallable),
			  _invoke(invoke)
		{}

		const TypeInfo& GetReturnType() const;
		std::span<const ParameterInfo> GetParams() const;
		bool IsConst() const;

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
				const auto result = Invoke(obj, args, TypedRef{.Type = &TypeOf<Referent*>(), .Data = &pointer, .IsConst = false});
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
		bool _constCallable = false;
		Invoker _invoke = nullptr;
	};
}
