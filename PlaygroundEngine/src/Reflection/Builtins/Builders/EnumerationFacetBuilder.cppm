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
		std::uint64_t EnumValueThunk(const void* obj)
		{
			return ToEnumeratorValue(*static_cast<const Enum*>(obj));
		}

		template <typename Enum>
		void EnumAssignThunk(void* obj, const std::uint64_t value)
		{
			*static_cast<Enum*>(obj) = FromEnumeratorValue<Enum>(value);
		}

		template <typename Enum>
		consteval EnumerationFacet MakeEnumerationFacet()
		{
			// Built from the enumerators rather than a thunk: the enumerator array is a program-lifetime
			// static so the facet's span stays valid once the facet is copied into the table.
			static constexpr auto Enumerators = MakeEnumeratorsFromType<^^Enum>();
			return EnumerationFacet(TypeReferenceTo<^^std::underlying_type_t<Enum>>(), Enumerators, &EnumValueThunk<Enum>, &EnumAssignThunk<Enum>);
		}
	}

	// Matched on the unqualified enum type only. A trait-constrained specialization, unlike the pattern-matched
	// ones, would otherwise also claim const E, whose facet the builder suppresses: the traits would then read a
	// facet that is not there. A cv node renders through the defaults instead.

	template <typename T>
	requires(std::is_enum_v<T> && std::same_as<T, std::remove_cv_t<T>>)
	struct TypeInfoTraits<T> : TypeInfoTraitsDefaults
	{
		static std::string Stringify(const T value)
		{
			const TypeInfo& typeInfo = TypeMetaOf<T>();
			const EnumerationFacet& facet = *typeInfo.GetFacet<EnumerationFacet>();
			if (const EnumeratorInfo* enumerator = facet.FindByValue(ToEnumeratorValue(value)))
			{
				return std::string(enumerator->GetIdentifier());
			}

			return ToString(static_cast<std::underlying_type_t<T>>(value));
		}

		static consteval auto MakeFacets()
		{
			return std::tuple{detail::MakeEnumerationFacet<T>()};
		}
	};
}
