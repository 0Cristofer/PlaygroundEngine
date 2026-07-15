export module PlaygroundEngine.Reflection.Core:StaticFieldInfo;

import :TypedRef;
import :TypeReference;
import :DeclarationInfo;
import :FieldInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	// A static data member has no offset and no instance, so every accessor drops the object pointer. That
	// is why it is its own metadata type with its own span on TypeInfo, not an entry in the field list.
	export using StaticFieldGetter = std::expected<void, FieldError> (*)(const TypedRef& out);
	export using StaticFieldSetter = std::expected<void, FieldError> (*)(const TypedRef& in);
	export using StaticFieldReferencer = TypedRef (*)();

	export struct StaticFieldTraits
	{
		AccessKind Access = AccessKind::Public;

		// A constant-readable member is captured by value at consteval and never odr-used, which is what
		// keeps a `static const int X = 5;` with no out-of-line definition from failing at link time.
		// The split is the semantics: constant-readable is read-only, addressable is settable.
		bool IsConstantReadable = false;
	};

	export class StaticFieldInfo : public DeclarationInfo
	{
	public:
		constexpr StaticFieldInfo(const TypeReference typeInfo,
								  const std::string_view identifier,
								  const std::string_view displayName,
								  const std::span<const std::string_view> scopePath,
								  const StaticFieldTraits& traits,
								  const StaticFieldGetter getter,
								  const StaticFieldSetter setter,
								  const StaticFieldReferencer referencer,
								  const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _typeInfo(typeInfo), _traits(traits), _getter(getter),
			  _setter(setter), _referencer(referencer)
		{}

		const TypeInfo& GetTypeInfo() const
		{
			return _typeInfo.Get();
		}

		const StaticFieldTraits& GetTraits() const
		{
			return _traits;
		}
		AccessKind GetAccess() const
		{
			return _traits.Access;
		}
		bool IsConstantReadable() const
		{
			return _traits.IsConstantReadable;
		}

		std::expected<void, FieldError> GetValue(const TypedRef& out) const
		{
			if (!_getter)
			{
				return std::unexpected(FieldError{FieldError::NotReadable});
			}

			return _getter(out);
		}

		std::expected<void, FieldError> SetValue(const TypedRef& in) const
		{
			if (!_setter)
			{
				return std::unexpected(FieldError{FieldError::NotWritable});
			}

			return _setter(in);
		}

		std::expected<TypedRef, FieldError> GetRef() const
		{
			if (!_referencer)
			{
				return std::unexpected(FieldError{FieldError::NotAddressable});
			}

			return _referencer();
		}

		template <typename T>
		std::expected<T, FieldError> GetAs() const
		{
			alignas(T) std::byte storage[sizeof(T)];
			if (const auto result = GetValue(TypedRef{.Type = &TypeOf<T>(), .Data = storage, .IsConst = false}); !result)
			{
				return std::unexpected(result.error());
			}

			T* pointer = std::launder(reinterpret_cast<T*>(storage));
			T value = std::move(*pointer);
			std::destroy_at(pointer);
			return value;
		}

		template <typename T>
		std::expected<void, FieldError> SetAs(const T& value) const
		{
			return SetValue(
				TypedRef{.Type = &TypeOf<T>(), .Data = const_cast<void*>(static_cast<const void*>(std::addressof(value))), .IsConst = true});
		}

	private:
		TypeReference _typeInfo;
		StaticFieldTraits _traits;
		StaticFieldGetter _getter = nullptr;
		StaticFieldSetter _setter = nullptr;
		StaticFieldReferencer _referencer = nullptr;
	};
}
