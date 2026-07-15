module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:BasesBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :BaseInfo;
import :DeclarationInfo;

import std;

// Builds the BaseInfo list for a type: its direct base classes, each with access, layout offset, and
// annotations. Virtual inheritance is rejected here so the offset stays a chainable layout constant.

namespace PgE::detail
{
	template <std::meta::info MetaBase>
	consteval BaseInfo MakeBase()
	{
		// A virtual base's derived-to-base distance lives in the vtable and depends on the most-derived type,
		// so a stored constant would be wrong once the type is embedded further down. Reject it at build time.
		static_assert(!std::meta::is_virtual(MetaBase), "reflected types may not use virtual inheritance");

		return BaseInfo(TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaBase))>(), AccessOf(MetaBase),
						std::meta::offset_of(MetaBase).bytes, MakeAnnotations<MetaBase>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeBaseArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto bases = std::define_static_array(std::meta::bases_of(MetaType, std::meta::access_context::unchecked()));
		return std::array<BaseInfo, sizeof...(I)>{MakeBase<bases[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeBasesFromType()
	{
		// A superseding facet (string, sequence, enum) views the type through the facet, not its structure, so
		// its bases are suppressed for the same reason its fields are.
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<BaseInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto baseCount = std::define_static_array(std::meta::bases_of(MetaType, std::meta::access_context::unchecked())).size();
			return MakeBaseArray<MetaType>(std::make_index_sequence<baseCount>{});
		}
		else
		{
			return std::array<BaseInfo, 0>{};
		}
	}
}
