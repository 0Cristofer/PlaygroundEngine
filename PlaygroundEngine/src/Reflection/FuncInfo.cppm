module;

#include <meta>

export module PlaygroundEngine.Reflection:FuncInfo;

import :TypedRef;

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

	export class ParamInfo
	{
	public:
		constexpr ParamInfo(const TypeInfo* typeInfo, const std::string_view name) :
			_typeInfo(typeInfo), _name(name)
		{
		}

		std::string_view GetName() const;
		const TypeInfo& GetTypeInfo() const;

	private:
		const TypeInfo* _typeInfo;
		std::string_view _name;
	};

	export class FuncInfo
	{
	public:
		constexpr FuncInfo(const TypeInfo* returnType, const std::string_view name,
		                   const std::span<const ParamInfo> params, const bool constCallable,
		                   const Invoker invoke) :
			_returnType(returnType), _name(name), _params(params), _constCallable(constCallable),
			_invoke(invoke)
		{
		}

		std::string_view GetName() const;
		const TypeInfo& GetReturnType() const;
		std::span<const ParamInfo> GetParams() const;
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
					.IsConst = std::is_const_v<std::remove_reference_t<Arguments>>
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
		std::string_view _name;
		std::span<const ParamInfo> _params;
		bool _constCallable = false;
		Invoker _invoke = nullptr;
	};
}
