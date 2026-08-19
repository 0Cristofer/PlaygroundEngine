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
	consteval ConversionInfo MakeConversion(const Invoker invoke)
	{
		return ConversionInfo(std::meta::is_explicit(MetaConversion),
							  TypeReferenceTo<std::meta::remove_cvref(std::meta::return_type_of(MetaConversion))>(), DisplayStringOf(MetaConversion),
							  ScopePathOf<MetaConversion>(), MakeFunctionTraits<MetaConversion>(), invoke, MakeAnnotations<MetaConversion>(),
							  TypeReferenceTo<MetaType>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<ConversionInfo, sizeof...(I)> MakeConversionArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto conversions = std::define_static_array(GetConversionFunctions(MetaType));

		// Invokers left null, set in place on demand (FillConversionInvokers), so the metadata build never splices.
		return std::array<ConversionInfo, sizeof...(I)>{MakeConversion<MetaType, conversions[I]>(nullptr)...};
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

	// One mutable array per type, instantiated only at demand (from MaterializeConversions), never on the
	// metadata walk. A ConversionInfo is a FunctionInfo, so SetInvoker fills its inherited invoker.
	template <std::meta::info MetaType>
	inline constinit auto GConversions = MakeConversionsFromType<MetaType>();

	// The view TypeInfo::GetConversions reads. Empty until MaterializeConversions publishes the list.
	template <std::meta::info MetaType>
	inline constinit std::span<const ConversionInfo> GConversionsView{};

	template <std::meta::info MetaType, std::size_t... I>
	void FillConversionInvokersImpl(std::index_sequence<I...>)
	{
		using T = [:MetaType:];
		[[maybe_unused]] constexpr auto conversions = std::define_static_array(GetConversionFunctions(MetaType));
		((SetInvoker(GConversions<MetaType>[I], MakeInvoker<T, conversions[I]>())), ...);
	}

	export template <std::meta::info MetaType>
	void MaterializeConversions()
	{
		// Publish the list (reifying the conversions) and fill the invokers, both at demand.
		GConversionsView<MetaType> = GConversions<MetaType>;

		constexpr auto count = std::define_static_array(GetConversionFunctions(MetaType)).size();
		FillConversionInvokersImpl<MetaType>(std::make_index_sequence<count>{});
	}
}
