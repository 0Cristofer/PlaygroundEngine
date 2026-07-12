module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:TypeBuilder;

import :TypeInfoTraits;

export import :MetaCommon;
export import :AnnotationsBuilder;
export import :FacetsBuilder;
export import :FieldsBuilder;
export import :FunctionsBuilder;
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
	consteval TypeInfo MakeType()
	{
		using T = [:MetaType:];
		constexpr std::string_view identifier = IdentifierOf(MetaType);
		constexpr std::string_view displayName = DisplayStringOf(MetaType);

		static constexpr auto Fields = MakeFieldsFromType<MetaType>();
		static constexpr auto Functions = MakeFunctionsFromType<MetaType>();

		static constexpr auto Annotations = MakeAnnotations<MetaType>();

		constexpr TypeTraits traits = MakeTraits<MetaType>();

		static constexpr auto Facets = MakeFacetsFromType<MetaType>();

		// A fieldless object is a leaf: it stringifies through its (total) trait, so it always gets a thunk.
		// is_object_v excludes void (a function return type reached here), where StringifyValue can't form a
		// const T*. A type with fields has no thunk and is rendered structurally by ObjectToString instead.
		if constexpr (Fields.empty() && std::is_object_v<T>)
		{
			return TypeInfo(identifier, displayName, Annotations, traits, Facets, Functions, Fields, &StringifyValue<T>);
		}
		else
		{
			return TypeInfo(identifier, displayName, Annotations, traits, Facets, Functions, Fields, nullptr);
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
