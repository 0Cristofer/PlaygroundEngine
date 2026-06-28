module;

#include <meta>

export module PlaygroundEngine.Reflection:Builder;

import PlaygroundEngine.Reflection.TypeInfo;
import PlaygroundEngine.Reflection.TypeInfoTraits;

import std;

// This file contains the compile-time reflection mechanisms to build the runtime reflection types

namespace PgE::detail
{
	template <std::meta::info MetaTypeInfo>
	constexpr const TypeInfo& TypeOfMeta();

	template <typename T>
	std::string StringifyValue(const void* obj)
	{
		return TypeInfoTraits<T>::Stringify(*static_cast<const T*>(obj));
	}

	template <const std::meta::info MetaMemberInfo>
	consteval FieldInfo MakeField()
	{
		const auto [bytes, bits] = std::meta::offset_of(MetaMemberInfo);
		return FieldInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaMemberInfo))>(),
		                 std::meta::display_string_of(MetaMemberInfo), bytes, bits);
	}

	template <std::meta::info MetaTypeInfo, std::size_t... I>
	consteval auto MakeNFieldsFromType(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(
			std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked()));
		return std::array<FieldInfo, sizeof...(I)>{MakeField<members[I]>()...};
	}

	template <std::meta::info MetaTypeInfo>
	consteval auto GetFieldsFromType()
	{
		using T = [:MetaTypeInfo:];
		if constexpr (TypeInfoTraits<T>::IS_LEAF)
		{
			return std::array<FieldInfo, 0>{};
		}
		else if constexpr (std::meta::is_class_type(MetaTypeInfo))
		{
			constexpr auto numMembers = std::define_static_array(
				std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked())).size();
			return MakeNFieldsFromType<MetaTypeInfo>(std::make_index_sequence<numMembers>{});
		}
		else
		{
			return std::array<FieldInfo, 0>{};
		}
	}

	// Reflected member functions, excluding constructors, destructors and operators.
	consteval std::vector<std::meta::info> GetMemberFunctions(const std::meta::info type)
	{
		std::vector<std::meta::info> functions;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			if (std::meta::is_function(member)
				&& !std::meta::is_special_member_function(member)
				&& !std::meta::is_constructor(member)
				&& !std::meta::is_destructor(member)
				&& !std::meta::is_operator_function(member))
			{
				functions.push_back(member);
			}
		}
		return functions;
	}

	template <const std::meta::info MetaParamInfo>
	consteval ParamInfo MakeParam()
	{
		constexpr std::string_view name = std::meta::has_identifier(MetaParamInfo)
			                                  ? std::meta::identifier_of(MetaParamInfo)
			                                  : std::string_view{};
		return ParamInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(MetaParamInfo))>(), name);
	}

	template <std::meta::info MetaFuncInfo, std::size_t... I>
	consteval auto MakeNParams(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto params = std::define_static_array(std::meta::parameters_of(MetaFuncInfo));
		return std::array<ParamInfo, sizeof...(I)>{MakeParam<params[I]>()...};
	}

	template <std::meta::info MetaFuncInfo>
	constexpr std::span<const ParamInfo> MakeParams()
	{
		constexpr auto numParams = std::meta::parameters_of(MetaFuncInfo).size();
		static constexpr auto params = MakeNParams<MetaFuncInfo>(std::make_index_sequence<numParams>{});
		return params;
	}

	template <const std::meta::info MetaFuncInfo>
	consteval FuncInfo MakeFunction()
	{
		return FuncInfo(&TypeOfMeta<std::meta::remove_cvref(std::meta::return_type_of(MetaFuncInfo))>(),
		                std::meta::identifier_of(MetaFuncInfo),
		                MakeParams<MetaFuncInfo>());
	}

	template <std::meta::info MetaTypeInfo, std::size_t... I>
	consteval auto MakeNFunctionsFromType(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto functions = std::define_static_array(GetMemberFunctions(MetaTypeInfo));
		return std::array<FuncInfo, sizeof...(I)>{MakeFunction<functions[I]>()...};
	}

	template <std::meta::info MetaTypeInfo>
	consteval auto GetFunctionsFromType()
	{
		using T = [:MetaTypeInfo:];
		if constexpr (TypeInfoTraits<T>::IS_LEAF)
		{
			return std::array<FuncInfo, 0>{};
		}
		else if constexpr (std::meta::is_class_type(MetaTypeInfo))
		{
			constexpr auto numFunctions = std::define_static_array(GetMemberFunctions(MetaTypeInfo)).size();
			return MakeNFunctionsFromType<MetaTypeInfo>(std::make_index_sequence<numFunctions>{});
		}
		else
		{
			return std::array<FuncInfo, 0>{};
		}
	}

	template <std::meta::info MetaTypeInfo>
	constexpr const TypeInfo& TypeOfMeta()
	{
		using T = [:MetaTypeInfo:];
		constexpr std::string_view displayName = std::meta::display_string_of(MetaTypeInfo);

		// ReSharper disable CppInconsistentNaming
		static constexpr auto fields = GetFieldsFromType<MetaTypeInfo>();
		static constexpr auto functions = GetFunctionsFromType<MetaTypeInfo>();

		if constexpr (fields.empty())
		{
			static constexpr TypeInfo typeInfo(displayName, fields, functions, &StringifyValue<T>);
			return typeInfo;
		}
		else
		{
			static constexpr TypeInfo typeInfo(displayName, fields, functions, nullptr);
			return typeInfo;
		}
		// ReSharper restore CppInconsistentNaming
	}
}
