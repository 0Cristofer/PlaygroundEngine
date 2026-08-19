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

			// The object handed to the access is not the type the field's thunks were built against. An
			// inherited field is reached through BaseInfo::Upcast, never through the derived object.
			ObjectTypeMismatch,

			// The borrow names no object to read the field out of. Distinct from NotAddressable, which is the
			// field's own answer (a bitfield has no address to borrow).
			NullObject,

			// A write reached through a read-only borrow. The type system cannot catch this once the object
			// is erased, so it is stated here.
			ConstViolation,
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
							const TypeReference declaringType,
							const std::string_view identifier,
							const std::string_view displayName,
							const std::span<const std::string_view> scopePath,
							const FieldTraits& traits,
							const FieldGetter getter,
							const FieldSetter setter,
							const FieldReferencer referencer,
							const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _typeInfo(typeInfo), _declaringType(declaringType), _traits(traits),
			  _getter(getter), _setter(setter), _referencer(referencer)
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
		// A mutable member is exempt from its object's constness by definition, and a reference member's
		// referent is a separate object the qualification never reached. Both stay writable through a const
		// object, so an erased access must not borrow them read-only either.
		bool WritableThroughConstObject() const
		{
			return _traits.IsMutable || _traits.IsLvalueReference || _traits.IsRvalueReference;
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

		const TypeInfo& GetDeclaringType() const;

		std::expected<void, FieldError> GetValue(const TypedRef& object, const TypedRef& out) const;
		std::expected<void, FieldError> SetValue(const TypedRef& object, const TypedRef& in) const;

		std::expected<TypedRef, FieldError> GetRef(const TypedRef& object) const;

		// The typed forms take the object itself, never its address: a pointer would erase as a pointer and
		// fail at runtime with ObjectTypeMismatch, so the constraint turns that into a compile error.
		template <typename T, typename Object>
		requires(!std::is_pointer_v<std::remove_cvref_t<Object>>)
		std::expected<T, FieldError> GetAs(const Object& object) const
		{
			alignas(T) std::byte storage[sizeof(T)];
			if (const auto result = GetValue(TypedRefOf(object), TypedRef{.Type = &TypeMetaOf<T>(), .Data = storage, .IsConst = false}); !result)
			{
				return std::unexpected(result.error());
			}

			T* pointer = std::launder(reinterpret_cast<T*>(storage));
			T value = std::move(*pointer);
			std::destroy_at(pointer);
			return value;
		}

		template <typename T, typename Object>
		requires(!std::is_pointer_v<std::remove_cvref_t<Object>>)
		std::expected<void, FieldError> SetAs(Object& object, const T& value) const
		{
			return SetValue(TypedRefOf(object), TypedRefOf(value));
		}

		template <typename T, typename Object>
		requires(!std::is_pointer_v<std::remove_cvref_t<Object>>)
		std::expected<void, FieldError> MoveAs(Object& object, T& value) const
		{
			return SetValue(TypedRefOf(object), TypedRefOf(std::move(value)));
		}

		template <typename T, typename Object>
		requires(!std::is_pointer_v<std::remove_cvref_t<Object>>)
		std::expected<std::reference_wrapper<T>, FieldError> GetRefAs(Object& object) const
		{
			const auto ref = GetRef(TypedRefOf(object));
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

		template <typename T, typename Object>
		requires(!std::is_pointer_v<std::remove_cvref_t<Object>>)
		std::expected<std::reference_wrapper<const T>, FieldError> GetRefAs(const Object& object) const
		{
			const auto ref = GetRef(TypedRefOf(object));
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
		std::expected<void, FieldError> CheckDeclaringInstance(const TypedRef& object) const;

		TypeReference _typeInfo;
		TypeReference _declaringType;
		FieldTraits _traits;
		FieldGetter _getter = nullptr;
		FieldSetter _setter = nullptr;
		FieldReferencer _referencer = nullptr;
	};
}
