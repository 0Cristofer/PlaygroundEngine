export module PlaygroundEngine.Reflection.Core:TypeInfo;

import :TypeInfoTraits;

import :FieldInfo;
import :StaticFieldInfo;
import :FunctionInfo;
import :TypedRef;
import :DeclarationInfo;
import :BaseInfo;
import :ConstructorInfo;
import :TemplateInfo;
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

	export enum class LinkageKind : std::uint8_t
	{
		None,
		Internal,
		Module,
		External,
	};

	export struct TypeTraits
	{
		TypeKind Kind = TypeKind::Other;

		// What lets a consumer refuse: a module- or internal-linkage type has no cross-translation-unit
		// identity by construction, so persisting a reference to one is a category error, and only
		// reflection can report it.
		LinkageKind Linkage = LinkageKind::None;

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
		bool IsFinal = false;

		bool IsTemplateInstance = false;

		bool IsSigned = false;
		bool IsScopedEnum = false;

		// The cv qualifiers of this type itself. A cv-qualified type is a decomposition node: it has no
		// identifier and no structure of its own, and names its unqualified type through GetInnerType.
		bool IsConst = false;
		bool IsVolatile = false;

		// Element count of an array type; 0 for an unbounded array.
		std::size_t Extent = 0;
	};

	export class TypeInfo : public DeclarationInfo
	{
	public:
		constexpr TypeInfo(const std::string_view identifier,
						   const std::string_view displayName,
						   const std::span<const std::string_view> scopePath,
						   const std::span<const AnnotationInfo> annotations,
						   const TypeTraits& traits,
						   const std::span<const FacetEntry> facets,
						   const std::span<const FunctionInfo> functions,
						   const std::span<const FieldInfo> fields,
						   const std::span<const StaticFieldInfo> staticFields,
						   const std::span<const BaseInfo> bases,
						   const std::span<const ConstructorInfo> constructors,
						   const TemplateInfo* templateInfo,
						   const std::span<const TemplateArgumentInfo> templateArguments,
						   const TypeReference innerType,
						   void (*destroyThunk)(void*),
						   std::string (*stringifyThunk)(const void*))
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _traits(traits), _facets(facets), _functions(functions),
			  _fields(fields), _staticFields(staticFields), _bases(bases), _constructors(constructors), _template(templateInfo),
			  _templateArguments(templateArguments), _innerType(innerType), _destroyThunk(destroyThunk), _stringifyThunk(stringifyThunk)
		{}

		const TypeTraits& GetTraits() const
		{
			return _traits;
		}
		TypeKind GetKind() const
		{
			return _traits.Kind;
		}
		std::size_t GetSize() const
		{
			return _traits.Size;
		}
		std::size_t GetAlignment() const
		{
			return _traits.Alignment;
		}

		std::span<const FacetEntry> GetFacets() const
		{
			return _facets;
		}
		template <typename Facet>
		const Facet* GetFacet() const
		{
			// Linear scan for now, this should be a small array
			for (const FacetEntry& entry : _facets)
			{
				if (&entry.Type.Get() == &TypeOf<Facet>())
				{
					return static_cast<const Facet*>(entry.Data);
				}
			}
			return nullptr;
		}

		std::span<const FunctionInfo> GetFunctions() const;
		std::vector<const FunctionInfo*> FindFunctionsByIdentifier(std::string_view identifier) const;

		std::span<const FieldInfo> GetFields() const
		{
			return _fields;
		}

		std::span<const StaticFieldInfo> GetStaticFields() const
		{
			return _staticFields;
		}

		const StaticFieldInfo* FindStaticFieldByIdentifier(std::string_view identifier) const;

		std::span<const BaseInfo> GetBases() const
		{
			return _bases;
		}

		// The primary template this type was instantiated from, null when it is not an instance. A template
		// is not a type, so it is named, not referenced: Grid<int> yields Grid.
		const TemplateInfo* GetTemplate() const
		{
			return _template;
		}

		std::span<const TemplateArgumentInfo> GetTemplateArguments() const
		{
			return _templateArguments;
		}

		// The type this compound type is built from: a pointer's pointee, a reference's referent, an array's
		// element, a cv-qualified type's unqualified type. Null for a type that decomposes no further, which
		// is where a walk bottoms out on a name. See docs/ReflectionInternals.md (compound types).
		bool HasInnerType() const
		{
			return _innerType.Resolve != nullptr;
		}
		const TypeInfo& GetInnerType() const pre(_innerType.Resolve != nullptr)
		{
			return _innerType.Get();
		}

		std::span<const ConstructorInfo> GetConstructors() const
		{
			return _constructors;
		}

		const ConstructorInfo* FindConstructor(ConstructorKind kind) const;
		const ConstructorInfo* FindConstructor(std::span<const TypedRef> args) const;

		std::expected<void, ConstructError> Construct(std::span<const TypedRef> args, const TypedRef& slot) const;

		template <typename T, typename... Arguments>
		std::expected<T, ConstructError> ConstructAs(Arguments&&... arguments) const
		{
			return detail::ValueFromSlot<T, ConstructError>(
				[this](const std::span<const TypedRef> args, const TypedRef& slot) { return Construct(args, slot); },
				detail::MakeTypedRefs(std::forward<Arguments>(arguments)...));
		}

		bool CanDestroy() const
		{
			return _destroyThunk;
		}
		void Destroy(void* obj) const pre(_destroyThunk != nullptr) pre(obj != nullptr)
		{
			_destroyThunk(obj);
		}

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
			{
				return std::unexpected(FieldError{FieldError::FieldNotFound});
			}

			return field->GetAs<T>(obj);
		}

		template <typename T>
		std::expected<void, FieldError> SetFieldAs(void* obj, const std::string_view identifier, const T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
			{
				return std::unexpected(FieldError{FieldError::FieldNotFound});
			}

			return field->SetAs<T>(obj, value);
		}

		template <typename T>
		std::expected<void, FieldError> MoveFieldAs(void* obj, const std::string_view identifier, T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
			{
				return std::unexpected(FieldError{FieldError::FieldNotFound});
			}

			return field->MoveAs<T>(obj, value);
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetFieldRefAs(void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
			{
				return std::unexpected(FieldError{FieldError::FieldNotFound});
			}

			return field->GetRefAs<T>(obj);
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetFieldRefAs(const void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
			{
				return std::unexpected(FieldError{FieldError::FieldNotFound});
			}

			return field->GetRefAs<T>(obj);
		}

		bool CanStringify() const
		{
			return _stringifyThunk;
		}
		std::string Stringify(const void* obj) const pre(_stringifyThunk != nullptr) pre(obj != nullptr)
		{
			return _stringifyThunk(obj);
		}

	private:
		std::expected<const ConstructorInfo*, ConstructError> SelectConstructor(std::span<const TypedRef> args) const;

		TypeTraits _traits;
		std::span<const FacetEntry> _facets;
		std::span<const FunctionInfo> _functions;
		std::span<const FieldInfo> _fields;
		std::span<const StaticFieldInfo> _staticFields;
		std::span<const BaseInfo> _bases;
		std::span<const ConstructorInfo> _constructors;
		const TemplateInfo* _template = nullptr;
		std::span<const TemplateArgumentInfo> _templateArguments;
		TypeReference _innerType;
		void (*_destroyThunk)(void*) = nullptr;
		std::string (*_stringifyThunk)(const void*) = nullptr;
	};
}
