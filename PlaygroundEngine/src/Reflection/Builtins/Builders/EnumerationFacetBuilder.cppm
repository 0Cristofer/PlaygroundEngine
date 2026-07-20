module;

#include <meta>

export module PlaygroundEngine.Reflection.Builtins:EnumerationFacetBuilder;

import PlaygroundEngine.Reflection.Core;

import :EnumerationFacet;
import :EnumeratorsBuilder;

import std;

namespace PgE
{
	namespace detail
	{
		template <typename Enum>
		consteval EnumerationFacet MakeEnumerationFacet()
		{
			// Built from the enumerators rather than a thunk: the enumerator array is a program-lifetime
			// static so the facet's span stays valid once the facet is copied into the table.
			static constexpr auto Enumerators = MakeEnumeratorsFromType<^^Enum>();
			return EnumerationFacet(TypeReferenceTo<^^std::underlying_type_t<Enum>>(), Enumerators);
		}
	}

	template <typename T>
	requires std::is_enum_v<T>
	struct TypeInfoTraits<T> : TypeInfoTraitsDefaults
	{
		static std::string Stringify(const T value)
		{
			const TypeInfo& typeInfo = TypeMetaOf<T>();
			const EnumerationFacet& facet = *typeInfo.GetFacet<EnumerationFacet>();
			const auto underlying = static_cast<std::underlying_type_t<T>>(value);

			if (const EnumeratorInfo* enumerator = facet.FindByValue(static_cast<std::uint64_t>(underlying)))
			{
				return std::string(enumerator->GetIdentifier());
			}

			return ToString(underlying);
		}

		static consteval auto MakeFacets()
		{
			return std::tuple{detail::MakeEnumerationFacet<T>()};
		}
	};
}
