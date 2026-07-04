export module PlaygroundEngine.Reflection:FieldInfo;

import :TypedRef;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export struct FieldError
	{
		enum Kind : std::uint8_t
		{
			FieldNotFound,
			TypeMismatch,
			NotReadable,
			NotWritable,
			NotAddressable,
		};

		Kind Reason;
	};

	export using FieldGetter = std::expected<void, FieldError> (*)(const void* obj, TypedRef out);
	export using FieldSetter = std::expected<void, FieldError> (*)(void* obj, TypedRef in);
	export using FieldReferencer = TypedRef (*)(void* obj);

	export class FieldInfo : public DeclarationInfo
	{
	public:
		constexpr FieldInfo(const TypeInfo* typeInfo, const std::string_view identifier,
		                    const std::string_view displayName, const int byteOffset, const int bitOffset,
		                    const FieldGetter getter, const FieldSetter setter,
		                    const FieldReferencer referencer,
		                    const std::span<const AnnotationInfo> annotations) :
			DeclarationInfo(identifier, displayName, annotations), _typeInfo(typeInfo),
			_byteOffset(byteOffset), _bitOffset(bitOffset), _getter(getter), _setter(setter),
			_referencer(referencer)
		{
		}

		int GetByteOffset() const;
		const TypeInfo& GetTypeInfo() const;

		std::expected<void, FieldError> GetValue(const void* obj, TypedRef out) const;
		std::expected<void, FieldError> SetValue(void* obj, TypedRef in) const;

		std::expected<TypedRef, FieldError> GetRef(void* obj) const;
		std::expected<TypedRef, FieldError> GetRef(const void* obj) const;

		template <typename T>
		std::expected<T, FieldError> GetAs(const void* obj) const
		{
			alignas(T) std::byte storage[sizeof(T)];
			if (const auto result = GetValue(obj, TypedRef{
				                                 .Type = &TypeOf<T>(), .Data = storage, .IsConst = false
			                                 }); !result)
				return std::unexpected(result.error());

			T* pointer = std::launder(reinterpret_cast<T*>(storage));
			T value = std::move(*pointer);
			std::destroy_at(pointer);
			return value;
		}

		template <typename T>
		std::expected<void, FieldError> SetAs(void* obj, const T& value) const
		{
			return SetValue(obj, TypedRef{
				                .Type = &TypeOf<T>(),
				                .Data = const_cast<void*>(static_cast<const void*>(std::addressof(value))),
				                .IsConst = true
			                });
		}

		template <typename T>
		std::expected<void, FieldError> MoveAs(void* obj, T& value) const
		{
			return SetValue(obj, TypedRef{
				                .Type = &TypeOf<T>(),
				                .Data = std::addressof(value),
				                .IsConst = false,
				                .Movable = true
			                });
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetRefAs(void* obj) const
		{
			const auto ref = GetRef(obj);
			if (!ref)
				return std::unexpected(ref.error());
			if (ref->Type != &TypeOf<T>())
				return std::unexpected(FieldError{FieldError::TypeMismatch});
			if (ref->IsConst)
				return std::unexpected(FieldError{FieldError::NotWritable});

			return std::reference_wrapper<T>(*static_cast<T*>(ref->Data));
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetRefAs(const void* obj) const
		{
			const auto ref = GetRef(obj);
			if (!ref)
				return std::unexpected(ref.error());
			if (ref->Type != &TypeOf<T>())
				return std::unexpected(FieldError{FieldError::TypeMismatch});

			return std::reference_wrapper<const T>(*static_cast<const T*>(ref->Data));
		}

	private:
		const TypeInfo* _typeInfo = nullptr;
		int _byteOffset;
		int _bitOffset;
		FieldGetter _getter = nullptr;
		FieldSetter _setter = nullptr;
		FieldReferencer _referencer = nullptr;
	};
}
