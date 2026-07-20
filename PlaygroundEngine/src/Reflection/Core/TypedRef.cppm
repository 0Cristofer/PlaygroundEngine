export module PlaygroundEngine.Reflection.Core:TypedRef;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeMetaOf();

	export struct TypedRef
	{
		const TypeInfo* Type = nullptr;
		void* Data = nullptr;
		bool IsConst = false;
		bool Movable = false;
	};

	namespace detail
	{
		// Erases a typed call's arguments for any sugar entry point (InvokeAs, ConstructAs). The value
		// category the caller wrote sets Movable, the binder's opt-in to move out of the argument. The refs
		// point at the caller's objects, so the array must not outlive the full expression that built it.
		export template <typename... Arguments>
		std::array<TypedRef, sizeof...(Arguments)> MakeTypedRefs(Arguments&&... arguments)
		{
			return {TypedRef{.Type = &TypeMetaOf<std::remove_cvref_t<Arguments>>(),
							 .Data = const_cast<void*>(static_cast<const void*>(std::addressof(arguments))),
							 .IsConst = std::is_const_v<std::remove_reference_t<Arguments>>,
							 .Movable = !std::is_lvalue_reference_v<Arguments>}...};
		}

		// The erased slot protocol, stated once: the callee builds its result into caller-owned storage, so
		// the value is laundered out and destroyed there. Shared by a function's return (InvokeAs) and a
		// constructor's object (ConstructAs), which differ only in the call and the error they report.
		export template <typename T, typename Error, typename SlotCall>
		std::expected<T, Error> ValueFromSlot(SlotCall&& call, const std::span<const TypedRef> args)
		{
			using Value = std::remove_cvref_t<T>;

			alignas(Value) std::byte storage[sizeof(Value)];
			const auto result = call(args, TypedRef{.Type = &TypeMetaOf<Value>(), .Data = storage, .IsConst = false});
			if (!result)
			{
				return std::unexpected(result.error());
			}

			Value* pointer = std::launder(reinterpret_cast<Value*>(storage));
			Value value = std::move(*pointer);
			std::destroy_at(pointer);
			return value;
		}
	}
}
