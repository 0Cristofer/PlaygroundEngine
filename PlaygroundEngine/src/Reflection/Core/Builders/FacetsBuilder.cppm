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
	consteval bool ProvidesFacets()
	{
		// Without this, a const enum would advertise an Assign it cannot honor, and a const T[N] a mutable element ref.
		if constexpr (std::meta::is_const_type(MetaType) || std::meta::is_volatile_type(MetaType))
		{
			return false;
		}
		else
		{
			// Asked before the traits are ever named, since deducing MakeFacets' return type instantiates its
			// body, and a body built for a cv type is exactly what must not be instantiated.
			using T = [:MetaType:];
			return requires { TypeInfoTraits<T>::MakeFacets(); };
		}
	}

	template <std::meta::info MetaType>
	consteval bool ProvidesSupersedingFacet()
	{
		// The builder-side rule that stops recursive reflection at protocol boundaries: a type whose facets
		// supersede gets empty field and function spans, so reflecting std::vector<T> emits its TypeInfo, its
		// facet, and recursion into T, nothing else. No facet kind is named here.
		if constexpr (ProvidesFacets<MetaType>())
		{
			using T = [:MetaType:];
			return AnyFacetSupersedes(TypeInfoTraits<T>::MakeFacets());
		}
		else
		{
			return false;
		}
	}

	// Stamps the providing type onto a facet that offers the hook, so a facet op can reject an object of
	// some other type. Read generically, the way Supersedes is: the builder still names no facet kind.
	template <std::meta::info MetaType, typename FacetType>
	consteval FacetType WithOwnerType(FacetType facet)
	{
		if constexpr (requires { facet.SetOwnerType(TypeReference{}); })
		{
			facet.SetOwnerType(TypeReferenceTo<MetaType>());
		}

		return facet;
	}

	template <std::meta::info MetaType, std::size_t Index>
	consteval FacetEntry MakeFacetEntry()
	{
		// One entry per facet the type provides, keyed by the facet's own type (the settled TypeInfo identity
		// rule) read off decltype, so the builder never names a facet kind. The facet is copied into a
		// program-lifetime static whose stable address goes into the entry.
		using T = [:MetaType:];
		static constexpr auto Facets = TypeInfoTraits<T>::MakeFacets();
		static constexpr auto Facet = WithOwnerType<MetaType>(std::get<Index>(Facets));
		using FacetType = std::remove_cvref_t<decltype(Facet)>;
		return FacetEntry{.Type = TypeReferenceTo<^^FacetType>(), .Data = &Facet};
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<FacetEntry, sizeof...(I)> MakeFacetEntryArray(std::index_sequence<I...>)
	{
		return std::array<FacetEntry, sizeof...(I)>{MakeFacetEntry<MetaType, I>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeFacetsFromType()
	{
		if constexpr (ProvidesFacets<MetaType>())
		{
			using T = [:MetaType:];
			constexpr auto count = std::tuple_size_v<decltype(TypeInfoTraits<T>::MakeFacets())>;
			return MakeFacetEntryArray<MetaType>(std::make_index_sequence<count>{});
		}
		else
		{
			return std::array<FacetEntry, 0>{};
		}
	}
}
