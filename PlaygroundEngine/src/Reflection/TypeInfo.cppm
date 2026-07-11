export module PlaygroundEngine.Reflection:TypeInfo;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :FieldInfo;
import :FunctionInfo;
import :TypedRef;
import :DeclarationInfo;
import :EnumerationFacet;
import :Facets;

import std;

namespace PgE
{
	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export enum class TypeKind : std::uint8_t
	{
		Void,
		NullPointer,
		Integral,
		FloatingPoint,
		Enum,
		Union,
		Class,
		Array,
		Pointer,
		MemberObjectPointer,
		MemberFunctionPointer,
		Function,
		LValueReference,
		RValueReference,
		Other,
	};

	export struct TypeTraits
	{
		TypeKind Kind = TypeKind::Other;

		std::size_t Size = 0;
		std::size_t Alignment = 0;

		bool IsTriviallyCopyable = false;
		bool IsTriviallyDefaultConstructible = false;
		bool IsTriviallyDestructible = false;
		bool IsStandardLayout = false;
		bool HasUniqueObjectRepresentations = false;

		bool IsDefaultConstructible = false;
		bool IsAggregate = false;
		bool IsPolymorphic = false;
		bool IsAbstract = false;
		bool HasVirtualDestructor = false;

		bool IsEmpty = false;

		bool IsTemplateInstance = false;

		bool IsSigned = false;
		bool IsScopedEnum = false;
	};

	export class TypeInfo : public DeclarationInfo
	{
	public:
		constexpr TypeInfo(const std::string_view identifier, const std::string_view displayName, const std::span<const AnnotationInfo> annotations,
		                   const TypeTraits& traits,
		                   const std::span<const FacetEntry> facets,
		                   const std::span<const FunctionInfo> functions,
		                   const std::span<const FieldInfo> fields,
		                   std::string (*stringifyThunk)(const void*)) :
			DeclarationInfo(identifier, displayName, annotations),
			_traits(traits),
			_facets(facets),
			_functions(functions),
			_fields(fields),
			_stringifyThunk(stringifyThunk)
		{
		}

		const TypeTraits& GetTraits() const { return _traits; }
		TypeKind GetKind() const { return _traits.Kind; }
		std::size_t GetSize() const { return _traits.Size; }
		std::size_t GetAlignment() const { return _traits.Alignment; }

		std::span<const FacetEntry> GetFacets() const { return _facets; }
		template <typename Facet>
		const Facet* GetFacet() const
		{
			// Linear scan for now, this should be a small array
			for (const FacetEntry& entry : _facets)
				if (&entry.Key.Get() == &TypeOf<Facet>())
					return static_cast<const Facet*>(entry.Data);
			return nullptr;
		}

		std::span<const FunctionInfo> GetFunctions() const;
		std::vector<const FunctionInfo*> FindFunctionsByIdentifier(std::string_view identifier) const;

		std::span<const FieldInfo> GetFields() const { return _fields; }
		const FieldInfo* FindFieldByIdentifier(std::string_view identifier) const;
		std::expected<void, FieldError> GetFieldValue(const void* obj, std::string_view identifier, const TypedRef& out) const;
		std::expected<void, FieldError> SetFieldValue(void* obj, std::string_view identifier, const TypedRef& in) const;
		std::expected<TypedRef, FieldError> GetFieldRef(void* obj, std::string_view identifier) const;
		std::expected<TypedRef, FieldError> GetFieldRef(const void* obj, std::string_view identifier) const;

		template <typename T>
		std::expected<T, FieldError> GetFieldAs(const void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetAs<T>(obj);
		}

		template <typename T>
		std::expected<void, FieldError> SetFieldAs(void* obj, const std::string_view identifier, const T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->SetAs<T>(obj, value);
		}

		template <typename T>
		std::expected<void, FieldError> MoveFieldAs(void* obj, const std::string_view identifier, T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->MoveAs<T>(obj, value);
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetFieldRefAs(void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetFieldRefAs(const void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

		bool CanStringify() const { return _stringifyThunk; }
		std::string Stringify(const void* obj) const { return _stringifyThunk(obj); }

	private:
		TypeTraits _traits;
		std::span<const FacetEntry> _facets;
		std::span<const FunctionInfo> _functions;
		std::span<const FieldInfo> _fields;
		std::string (*_stringifyThunk)(const void*) = nullptr;
	};
}
