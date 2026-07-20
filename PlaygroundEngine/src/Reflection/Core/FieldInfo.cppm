export module PlaygroundEngine.Reflection.Core:FieldInfo;

import :TypedRef;
import :TypeReference;
import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeMetaOf();

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

	export using FieldGetter = std::expected<void, FieldError> (*)(const void* obj, const TypedRef& out);
	export using FieldSetter = std::expected<void, FieldError> (*)(void* obj, const TypedRef& in);
	export using FieldReferencer = TypedRef (*)(void* obj);

	export struct FieldTraits
	{
		// The layout and language facts of one non-static data member, grouped so the FieldInfo constructor
		// stays readable and each fact is named at the call site (the same shape TypeTraits uses on TypeInfo).

		AccessKind Access = AccessKind::Public;

		int ByteOffset = 0;
		int BitOffset = 0;

		// Width in bits, meaningful only when IsBitField. Without it a bitfield cannot be serialized,
		// delta-encoded, or layout-matched against a shader.
		bool IsBitField = false;
		int BitSize = 0;

		// A defaulted member is what lets a serializer omit an unchanged value from a text asset, which is
		// what keeps assets small and mergeable.
		bool HasDefaultInitializer = false;

		bool IsMutable = false;

		// The cv and reference qualifiers the stored decayed TypeReference loses: what tells an owned value
		// (serialize inline) from a cross-reference (never inline), int& Alias from const int& ConstAlias from
		// int Target, which share one tag. Same shape as ParameterTraits, same reason.
		bool IsConst = false;
		bool IsVolatile = false;
		bool IsLvalueReference = false;
		bool IsRvalueReference = false;
	};

	export class FieldInfo : public DeclarationInfo
	{
	public:
		constexpr FieldInfo(const TypeReference typeInfo,
							const std::string_view identifier,
							const std::string_view displayName,
							const std::span<const std::string_view> scopePath,
							const FieldTraits& traits,
							const FieldGetter getter,
							const FieldSetter setter,
							const FieldReferencer referencer,
							const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _typeInfo(typeInfo), _traits(traits), _getter(getter),
			  _setter(setter), _referencer(referencer)
		{}

		const FieldTraits& GetTraits() const
		{
			return _traits;
		}
		int GetByteOffset() const;
		int GetBitOffset() const
		{
			return _traits.BitOffset;
		}
		bool IsBitField() const
		{
			return _traits.IsBitField;
		}
		int GetBitSize() const
		{
			return _traits.BitSize;
		}
		AccessKind GetAccess() const
		{
			return _traits.Access;
		}
		bool HasDefaultInitializer() const
		{
			return _traits.HasDefaultInitializer;
		}
		bool IsMutable() const
		{
			return _traits.IsMutable;
		}
		bool IsConst() const
		{
			return _traits.IsConst;
		}
		bool IsVolatile() const
		{
			return _traits.IsVolatile;
		}
		bool IsLvalueReference() const
		{
			return _traits.IsLvalueReference;
		}
		bool IsRvalueReference() const
		{
			return _traits.IsRvalueReference;
		}

		const TypeInfo& GetTypeInfo() const;

		std::expected<void, FieldError> GetValue(const void* obj, const TypedRef& out) const;
		std::expected<void, FieldError> SetValue(void* obj, const TypedRef& in) const;

		std::expected<TypedRef, FieldError> GetRef(void* obj) const;
		std::expected<TypedRef, FieldError> GetRef(const void* obj) const;

		template <typename T>
		std::expected<T, FieldError> GetAs(const void* obj) const
		{
			alignas(T) std::byte storage[sizeof(T)];
			if (const auto result = GetValue(obj, TypedRef{.Type = &TypeMetaOf<T>(), .Data = storage, .IsConst = false}); !result)
			{
				return std::unexpected(result.error());
			}

			T* pointer = std::launder(reinterpret_cast<T*>(storage));
			T value = std::move(*pointer);
			std::destroy_at(pointer);
			return value;
		}

		template <typename T>
		std::expected<void, FieldError> SetAs(void* obj, const T& value) const
		{
			return SetValue(
				obj, TypedRef{.Type = &TypeMetaOf<T>(), .Data = const_cast<void*>(static_cast<const void*>(std::addressof(value))), .IsConst = true});
		}

		template <typename T>
		std::expected<void, FieldError> MoveAs(void* obj, T& value) const
		{
			return SetValue(obj, TypedRef{.Type = &TypeMetaOf<T>(), .Data = std::addressof(value), .IsConst = false, .Movable = true});
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetRefAs(void* obj) const
		{
			const auto ref = GetRef(obj);
			if (!ref)
			{
				return std::unexpected(ref.error());
			}
			if (ref->Type != &TypeMetaOf<T>())
			{
				return std::unexpected(FieldError{FieldError::TypeMismatch});
			}
			if (ref->IsConst)
			{
				return std::unexpected(FieldError{FieldError::NotWritable});
			}

			return std::reference_wrapper<T>(*static_cast<T*>(ref->Data));
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetRefAs(const void* obj) const
		{
			const auto ref = GetRef(obj);
			if (!ref)
			{
				return std::unexpected(ref.error());
			}
			if (ref->Type != &TypeMetaOf<T>())
			{
				return std::unexpected(FieldError{FieldError::TypeMismatch});
			}

			return std::reference_wrapper<const T>(*static_cast<const T*>(ref->Data));
		}

	private:
		TypeReference _typeInfo;
		FieldTraits _traits;
		FieldGetter _getter = nullptr;
		FieldSetter _setter = nullptr;
		FieldReferencer _referencer = nullptr;
	};
}
