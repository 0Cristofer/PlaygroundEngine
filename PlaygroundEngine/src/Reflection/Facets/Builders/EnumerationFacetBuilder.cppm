module;

#include <meta>

export module PlaygroundEngine.Reflection:EnumerationFacetBuilder;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :MetaCommon;
import :EnumerationFacet;
import :EnumeratorsBuilder;

import std;

// The pre-made enumeration facet and the enum TypeInfoTraits specialization. An enum exposes an
// EnumerationFacet (the enumerator set plus the underlying integer type); its enumerator-name rendering
// is done by ObjectToString through that facet, not a per-enum stringify thunk. It lives here, like the
// string and sequence facets, because MakeFacets names EnumerationFacet, a reflection partition the
// standalone TypeInfoTraits module cannot import.

namespace PgE
{
	namespace detail
	{
		template <typename Enum>
		std::uint64_t ReadEnumValue(const void* obj)
		{
			// Mirrors the enumerator capture (EnumeratorValueOf): read the enum, narrow to its underlying
			// integer, widen to uint64_t as a raw bit pattern so a signed negative wraps to two's complement.
			return static_cast<std::uint64_t>(static_cast<std::underlying_type_t<Enum>>(*static_cast<const Enum*>(obj)));
		}

		template <typename Enum>
		consteval EnumerationFacet MakeEnumerationFacet()
		{
			// Built from the enumerators rather than a thunk: the enumerator array is a program-lifetime
			// static so the facet's span stays valid once the facet is copied into the table.
			static constexpr auto Enumerators = MakeEnumeratorsFromType<^^Enum>();
			return EnumerationFacet(TypeReferenceTo<^^std::underlying_type_t<Enum>>(), Enumerators,
			                        &ReadEnumValue<Enum>);
		}
	}

	template <typename T>
		requires std::is_enum_v<T>
	struct TypeInfoTraits<T> : TypeInfoTraitsDefaults
	{
		static consteval auto MakeFacets()
		{
			return std::tuple{detail::MakeEnumerationFacet<T>()};
		}
	};
}
