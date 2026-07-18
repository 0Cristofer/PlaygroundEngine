module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:ConversionsBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :ArgumentBinding;
import :FunctionsBuilder;
import :TypeInfo;
import :ConversionInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

// Builds the ConversionInfo list for a type. A conversion function reuses the function machinery (its target
// is its return type, its invoke thunk reads the object and produces the target); this only adds the explicit
// flag and keeps conversions in their own list instead of leaking into GetFunctions nameless.

namespace PgE::detail
{
	consteval std::vector<std::meta::info> GetConversionFunctions(const std::meta::info type)
	{
		std::vector<std::meta::info> conversions;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_conversion_function(member))
			{
				conversions.push_back(member);
			}
		}
		return conversions;
	}

	template <std::meta::info MetaType, std::meta::info MetaConversion>
	consteval ConversionInfo MakeConversion()
	{
		using T = [:MetaType:];

		return ConversionInfo(std::meta::is_explicit(MetaConversion),
							  TypeReferenceTo<std::meta::remove_cvref(std::meta::return_type_of(MetaConversion))>(), DisplayStringOf(MetaConversion),
							  ScopePathOf<MetaConversion>(), MakeFunctionTraits<MetaConversion>(), MakeInvoker<T, MetaConversion>(),
							  MakeAnnotations<MetaConversion>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeConversionArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto conversions = std::define_static_array(GetConversionFunctions(MetaType));
		return std::array<ConversionInfo, sizeof...(I)>{MakeConversion<MetaType, conversions[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeConversionsFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<ConversionInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto conversionCount = std::define_static_array(GetConversionFunctions(MetaType)).size();
			return MakeConversionArray<MetaType>(std::make_index_sequence<conversionCount>{});
		}
		else
		{
			return std::array<ConversionInfo, 0>{};
		}
	}
}
