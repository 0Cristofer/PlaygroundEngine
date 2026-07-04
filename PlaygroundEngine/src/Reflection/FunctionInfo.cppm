export module PlaygroundEngine.Reflection:FunctionInfo;

import :TypedRef;
import :DeclarationInfo;

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
			ConstViolation,
			ReturnTypeMismatch,
		};

		Kind Reason;
		std::uint16_t ArgumentIndex = 0;
	};

	export using Invoker =
	std::expected<void, InvokeError> (*)(void* obj, std::span<const TypedRef> args, TypedRef ret);

	export template <typename Return>
	using InvokeResult = std::conditional_t<std::is_reference_v<Return>,
	                                        std::reference_wrapper<std::remove_reference_t<Return>>, Return>;

	export class ParameterInfo : public DeclarationInfo
	{
	public:
		constexpr ParameterInfo(const TypeInfo* typeInfo, const std::string_view identifier,
		                    const std::string_view displayName,
		                    const std::span<const AnnotationInfo> annotations) :
			DeclarationInfo(identifier, displayName, annotations), _typeInfo(typeInfo)
		{
		}

		const TypeInfo& GetTypeInfo() const;

	private:
		const TypeInfo* _typeInfo;
	};

	export class FunctionInfo : public DeclarationInfo
	{
	public:
		constexpr FunctionInfo(const TypeInfo* returnType, const std::string_view identifier,
		                   const std::string_view displayName, const std::span<const ParameterInfo> params,
		                   const bool constCallable, const Invoker invoke,
		                   const std::span<const AnnotationInfo> annotations) :
			DeclarationInfo(identifier, displayName, annotations), _returnType(returnType),
			_params(params), _constCallable(constCallable), _invoke(invoke)
		{
		}

		const TypeInfo& GetReturnType() const;
		std::span<const ParameterInfo> GetParams() const;
		bool IsConst() const;

		std::expected<void, InvokeError> Invoke(void* obj, std::span<const TypedRef> args,
		                                        TypedRef ret = {}) const;
		std::expected<void, InvokeError> Invoke(const void* obj, std::span<const TypedRef> args,
		                                        TypedRef ret = {}) const;

		template <typename Return = void, typename Object, typename... Arguments>
		std::expected<InvokeResult<Return>, InvokeError> InvokeAs(Object* obj, Arguments&&... arguments) const
		{
			std::array<TypedRef, sizeof...(Arguments)> args{
				TypedRef{
					.Type = &TypeOf<std::remove_cvref_t<Arguments>>(),
					.Data = const_cast<void*>(static_cast<const void*>(std::addressof(arguments))),
					.IsConst = std::is_const_v<std::remove_reference_t<Arguments>>,
					.Movable = !std::is_lvalue_reference_v<Arguments>
				}...
			};

			if constexpr (std::is_void_v<Return>)
			{
				return Invoke(obj, args);
			}
			else if constexpr (std::is_reference_v<Return>)
			{
				using Referent = std::remove_reference_t<Return>;
				Referent* pointer = nullptr;
				const auto result = Invoke(obj, args, TypedRef{
					                           .Type = &TypeOf<Referent*>(), .Data = &pointer, .IsConst = false
				                           });
				if (!result)
					return std::unexpected(result.error());

				return std::reference_wrapper<Referent>(*pointer);
			}
			else
			{
				alignas(Return) std::byte storage[sizeof(Return)];
				const auto result =
					Invoke(obj, args, TypedRef{
						       .Type = &TypeOf<std::remove_cvref_t<Return>>(), .Data = storage, .IsConst = false
					       });
				if (!result)
					return std::unexpected(result.error());

				Return* pointer = std::launder(reinterpret_cast<Return*>(storage));
				Return value = std::move(*pointer);
				std::destroy_at(pointer);
				return value;
			}
		}

	private:
		const TypeInfo* _returnType = nullptr;
		std::span<const ParameterInfo> _params;
		bool _constCallable = false;
		Invoker _invoke = nullptr;
	};
}
