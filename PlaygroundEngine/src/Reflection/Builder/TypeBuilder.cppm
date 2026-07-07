module;

#include <meta>

export module PlaygroundEngine.Reflection:TypeBuilder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

export import :MetaCommon;
export import :AnnotationsBuilder;
export import :FieldsBuilder;
export import :FunctionsBuilder;
export import :TraitsBuilder;
export import :EnumeratorsBuilder;
import :TypeInfo;
import :EnumerationInfo;

import std;

// The home for building a type's runtime TypeInfo: it orchestrates the per-kind builders (fields,
// functions, annotations, and later bases, size, enumerators) into one TypeInfo. New kinds of type
// information are added here, delegating collection-heavy work to their own :*Builder partition.
//
// It also holds the sole definition of the TypeOfMeta recursion knot (declared in :MetaCommon).
// Instantiating it here pulls in the sub-builders, which recurse back into TypeOfMeta for member and
// signature types; the definition being visible only in this partition is what keeps the sub-builders
// dependent on the declaration alone, so the partition import graph stays acyclic.

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

		if constexpr (std::meta::is_enum_type(MetaType))
		{
			static constexpr auto Enumerators = MakeEnumeratorsFromType<MetaType>();
			static constexpr EnumerationInfo Enumeration(&TypeOfMeta<^^std::underlying_type_t<T>>(),
			                                             Enumerators);

			// An enum renders as its enumerator name through TypeInfoTraits, reached by the same
			// StringifyValue thunk every other leaf uses; this branch exists only to attach the facet.
			return TypeInfo(identifier, displayName, traits, Fields, Functions, &StringifyValue<T>,
			                Annotations, &Enumeration);
		}
		// std::is_object_v excludes void (reached here as a function return type), where
		// StringifyValue can't form a const T*; it stays true for primitives and classes.
		else if constexpr (Fields.empty() && std::is_object_v<T>)
		{
			return TypeInfo(identifier, displayName, traits, Fields, Functions, &StringifyValue<T>,
			                Annotations);
		}
		else
		{
			return TypeInfo(identifier, displayName, traits, Fields, Functions, nullptr, Annotations);
		}
	}

	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta()
	{
		// Key the cached TypeInfo on the canonical type, not the spelling: every alias of a type
		// (std::uint16_t, std::underlying_type_t<E>, unsigned short) must resolve to one instance, so the
		// pointer identity that annotation matching, serialization, and C# dedup rely on holds regardless
		// of how a type was named at the query site.
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
