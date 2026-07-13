module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:FacetsBuilder;

import :TypeInfoTraits;

import :MetaCommon;
import :Facets;

import std;

// Assembles the type-erased facet table generically, naming no facet kind: it keys each entry by the
// facet's own type and reads its Supersedes flag off that type. Adding a facet is a new TypeInfoTraits
// specialization, never an edit here. See docs/ReflectionInternals.md (Facets).

namespace PgE::detail
{
	template <typename Facet>
	consteval bool FacetSupersedes()
	{
		// A facet declares whether it supersedes the structural view; one that adds information alongside the
		// fields (a future provenance facet) simply omits the member and reads as false.
		if constexpr (requires { Facet::Supersedes; })
		{
			return Facet::Supersedes;
		}
		else
		{
			return false;
		}
	}

	template <typename... Facets>
	consteval bool AnyFacetSupersedes(const std::tuple<Facets...>&)
	{
		return (FacetSupersedes<Facets>() || ...);
	}

	template <std::meta::info MetaType>
	consteval bool ProvidesSupersedingFacet()
	{
		// The builder-side rule that stops recursive reflection at protocol boundaries: a type whose facets
		// supersede gets empty field and function spans, so reflecting std::vector<T> emits its TypeInfo, its
		// facet, and recursion into T, nothing else. No facet kind is named here.
		using T = [:MetaType:];
		if constexpr (requires { TypeInfoTraits<T>::MakeFacets(); })
		{
			return AnyFacetSupersedes(TypeInfoTraits<T>::MakeFacets());
		}
		else
		{
			return false;
		}
	}

	template <std::meta::info MetaType, std::size_t Index>
	consteval FacetEntry MakeFacetEntry()
	{
		// One entry per facet the type provides, keyed by the facet's own type (the settled TypeInfo identity
		// rule) read off decltype, so the builder never names a facet kind. The facet is copied into a
		// program-lifetime static whose stable address goes into the entry.
		using T = [:MetaType:];
		static constexpr auto Facets = TypeInfoTraits<T>::MakeFacets();
		static constexpr auto Facet = std::get<Index>(Facets);
		using FacetType = std::remove_cvref_t<decltype(Facet)>;
		return FacetEntry{.Key = TypeReferenceTo<^^FacetType>(), .Data = &Facet};
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeFacetEntryArray(std::index_sequence<I...>)
	{
		return std::array<FacetEntry, sizeof...(I)>{MakeFacetEntry<MetaType, I>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeFacetsFromType()
	{
		using T = [:MetaType:];
		if constexpr (requires { TypeInfoTraits<T>::MakeFacets(); })
		{
			constexpr auto count = std::tuple_size_v<decltype(TypeInfoTraits<T>::MakeFacets())>;
			return MakeFacetEntryArray<MetaType>(std::make_index_sequence<count>{});
		}
		else
		{
			return std::array<FacetEntry, 0>{};
		}
	}
}
