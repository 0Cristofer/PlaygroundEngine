module;

#include <meta>

export module PlaygroundEngine.Reflection.TypeInfo;

export import :Field;

import std;

namespace PlaygroundEngine
{
	export class TypeInfo
	{
	public:
		constexpr TypeInfo(std::string_view name, std::span<const Field> fields) : _displayName(name), _fields(fields)
		{

		}

		template <typename T>
		static constexpr const TypeInfo& TypeOf()
		{
			return TypeOf<^^T>();
		}

	private:
		template <std::meta::info MetaTypeInfo>
		static constexpr const TypeInfo& TypeOf()
		{
			constexpr std::string_view displayName = std::meta::display_string_of(MetaTypeInfo);

			static constexpr auto fields = GetFieldsFromType<MetaTypeInfo>();
			static constexpr TypeInfo typeInfo(displayName, fields);

			return typeInfo;
		}

		template <std::meta::info MetaTypeInfo>
		static consteval auto GetFieldsFromType()
		{
			if constexpr (std::meta::is_class_type(MetaTypeInfo))
			{
				constexpr auto numMembers = std::define_static_array(
				std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked())).size();
				return MakeNFieldsFromType<MetaTypeInfo>(std::make_index_sequence<numMembers>{});
			}
			else
			{
				return std::array<Field, 0>{};
			}
		}

		template <std::meta::info MetaTypeInfo, std::size_t... I>
		static consteval auto MakeNFieldsFromType(std::index_sequence<I...>)
		{
			constexpr auto members = std::define_static_array(
				std::meta::nonstatic_data_members_of(MetaTypeInfo, std::meta::access_context::unchecked()));
			return std::array<Field, sizeof...(I)>{ MakeField<members[I]>()...};
		}

		template <const std::meta::info MetaMemberInfo>
		static consteval Field MakeField()
		{
			const auto [bytes, bits] = std::meta::offset_of(MetaMemberInfo);
			return Field(&TypeOf<std::meta::type_of(MetaMemberInfo)>(), std::meta::display_string_of(MetaMemberInfo), bytes, bits);
		}

		std::string_view _displayName;
		std::span<const Field> _fields;
	};
}
