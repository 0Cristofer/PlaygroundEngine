module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:TypeBuilder;

import :TypeInfoTraits;

export import :MetaCommon;
export import :AnnotationsBuilder;
export import :ArgumentBinding;
export import :FacetsBuilder;
export import :FieldsBuilder;
export import :StaticFieldsBuilder;
export import :FunctionsBuilder;
export import :BasesBuilder;
export import :ConstructorsBuilder;
export import :TemplateBuilder;
export import :TraitsBuilder;
import :TypeInfo;
import :Facets;

import std;

// Builds a type's runtime TypeInfo by orchestrating the per-kind builders, and holds the sole definition
// of the TypeOfMeta recursion knot (declared in :MetaCommon). See docs/ReflectionInternals.md (the
// recursion knot, the builder pipeline).

namespace PgE::detail
{
	template <std::meta::info MetaType>
	consteval TypeReference MakeInnerType()
	{
		// Peel exactly one shape, so a walk chains: const Foo* yields const Foo, which yields Foo. cv is
		// peeled before the others, since a const pointer is a cv node whose inner is the pointer.
		if constexpr (std::meta::is_const_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_const(MetaType)>();
		}
		else if constexpr (std::meta::is_volatile_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_volatile(MetaType)>();
		}
		else if constexpr (std::meta::is_pointer_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_pointer(MetaType)>();
		}
		else if constexpr (std::meta::is_reference_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_reference(MetaType)>();
		}
		else if constexpr (std::meta::is_array_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_extent(MetaType)>();
		}
		else
		{
			return TypeReference{};
		}
	}

	template <std::meta::info MetaType>
	consteval TypeInfo MakeType()
	{
		using T = [:MetaType:];
		constexpr std::string_view identifier = TypeIdentifierOf(MetaType);
		constexpr std::string_view displayName = DisplayStringOf(MetaType);

		static constexpr auto Fields = MakeFieldsFromType<MetaType>();
		static constexpr auto StaticFields = MakeStaticFieldsFromType<MetaType>();
		static constexpr auto Functions = MakeFunctionsFromType<MetaType>();
		static constexpr auto Bases = MakeBasesFromType<MetaType>();
		static constexpr auto Constructors = MakeConstructorsFromType<MetaType>();

		static constexpr auto Annotations = MakeAnnotations<MetaType>();

		constexpr TypeTraits traits = MakeTraits<MetaType>();

		static constexpr auto Facets = MakeFacetsFromType<MetaType>();

		// A fieldless object is a leaf: it stringifies through its (total) trait, so it always gets a thunk.
		// The guard excludes void (a function return type reached here) and an incomplete type (an opaque
		// handle named by a pointer), neither of which StringifyValue can dereference.
		if constexpr (Fields.empty() && std::is_object_v<T> && std::meta::is_complete_type(MetaType))
		{
			return TypeInfo(identifier, displayName, ScopePathOf<MetaType>(), Annotations, traits, Facets, Functions, Fields, StaticFields, Bases,
							Constructors, MakeTemplate<MetaType>(), MakeTemplateArguments<MetaType>(), MakeInnerType<MetaType>(),
							MakeDestroyer<MetaType>(), &StringifyValue<T>);
		}
		else
		{
			return TypeInfo(identifier, displayName, ScopePathOf<MetaType>(), Annotations, traits, Facets, Functions, Fields, StaticFields, Bases,
							Constructors, MakeTemplate<MetaType>(), MakeTemplateArguments<MetaType>(), MakeInnerType<MetaType>(),
							MakeDestroyer<MetaType>(), nullptr);
		}
	}

	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta()
	{
		// Key the cached TypeInfo on the canonical (dealiased) type, so every alias resolves to one instance
		// and pointer identity holds. See docs/ReflectionInternals.md (canonical caching and dealiasing).
		if constexpr (std::meta::dealias(MetaType) != MetaType)
		{
			return TypeOfMeta<std::meta::dealias(MetaType)>();
		}
		else
		{
			static constexpr TypeInfo Type = MakeType<MetaType>();
			return Type;
		}
	}
}
