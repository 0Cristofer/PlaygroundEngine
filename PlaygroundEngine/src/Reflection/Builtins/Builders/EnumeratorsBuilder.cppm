module;

#include <meta>

export module PlaygroundEngine.Reflection.Builtins:EnumeratorsBuilder;

import PlaygroundEngine.Reflection.Core;

import :EnumerationFacet;

import std;

// Builds the EnumeratorInfo list for an enumeration type from std::meta::enumerators_of. The enumerator
// value is captured as the raw bit pattern of the underlying integer, widened to uint64_t; the underlying
// type on the EnumerationFacet says how to read it back.

namespace PgE::detail
{
	template <std::meta::info Enumerator>
	struct EnumeratorValueOf
	{
		using Enum = [:std::meta::type_of(Enumerator):];
		using Underlying = std::underlying_type_t<Enum>;

		static_assert(sizeof(Underlying) <= sizeof(std::uint64_t),
					  "Enumerator value does not fit in uint64_t: enum underlying type exceeds 64 bits "
					  "(a GCC extended-integer base). Such an enum cannot cross the C# boundary either.");

		static constexpr std::uint64_t Value = static_cast<std::uint64_t>(static_cast<Underlying>([:Enumerator:]));
	};

	template <std::meta::info Enumerator>
	consteval EnumeratorInfo MakeEnumerator()
	{
		return EnumeratorInfo(IdentifierOf(Enumerator), DisplayStringOf(Enumerator), EnumeratorValueOf<Enumerator>::Value,
							  MakeAnnotations<Enumerator>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeEnumeratorArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(MetaType));
		return std::array<EnumeratorInfo, sizeof...(I)>{MakeEnumerator<enumerators[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeEnumeratorsFromType()
	{
		constexpr auto count = std::define_static_array(std::meta::enumerators_of(MetaType)).size();
		return MakeEnumeratorArray<MetaType>(std::make_index_sequence<count>{});
	}
}
