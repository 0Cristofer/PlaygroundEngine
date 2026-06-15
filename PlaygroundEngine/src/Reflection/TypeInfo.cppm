module;

#include <meta>

export module PlaygroundEngine.Reflection.TypeInfo;

export import :Field;
export import :TypeInfoTraits;

import std;

namespace PlaygroundEngine
{
	export class TypeInfo
	{
	public:
		constexpr TypeInfo(const std::string_view name, const std::span<const Field> fields,
		                   std::string (*stringifyThunk)(const void*)) : _displayName(name),
		                                                                 _fields(fields),
		                                                                 _stringifyThunk(stringifyThunk)
		{
		}

		template <typename T>
		static constexpr const TypeInfo& TypeOf()
		{
			return TypeOf<^^T>();
		}

		template <typename T>
		static std::string ToStringTyped(const T& obj)
		{
			const TypeInfo& typeOfT = TypeOf<T>();

			return typeOfT.ToStringObj(&obj);
		}

		std::string ToStringObj(const void* obj) const
		{
			if (_stringifyThunk)
				return _stringifyThunk(obj);

			std::string out = "{";
			for (const Field& f : _fields)
			{
				const void* addr = static_cast<const std::byte*>(obj) + f.GetByteOffset();
				out += f.GetName();
				out += ": ";
				out += f.GetTypeInfo().ToStringObj(addr);
				out += ", ";
			}
			return out + "}";
		}

	private:
		template <std::meta::info MetaTypeInfo>
		static constexpr const TypeInfo& TypeOf()
		{
			using T = [:MetaTypeInfo:];
			constexpr std::string_view displayName = std::meta::display_string_of(MetaTypeInfo);

			static constexpr auto fields = GetFieldsFromType<MetaTypeInfo>();

			if constexpr (fields.empty())
			{
				static constexpr TypeInfo typeInfo(displayName, fields, &TypeInfo::StringifyValue<T>);
				return typeInfo;
			}
			else
			{
				static constexpr TypeInfo typeInfo(displayName, fields, nullptr);
				return typeInfo;
			}
		}

		template <std::meta::info MetaTypeInfo>
		static consteval auto GetFieldsFromType()
		{
			using T = [:MetaTypeInfo:];
			if constexpr (TypeInfoTraits<T>::IsLeaf)
			{
				return std::array<Field, 0>{};
			}
			else if constexpr (std::meta::is_class_type(MetaTypeInfo))
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
			return std::array<Field, sizeof...(I)>{MakeField<members[I]>()...};
		}

		template <const std::meta::info MetaMemberInfo>
		static consteval Field MakeField()
		{
			const auto [bytes, bits] = std::meta::offset_of(MetaMemberInfo);
			return Field(&TypeOf<std::meta::type_of(MetaMemberInfo)>(), std::meta::display_string_of(MetaMemberInfo),
			             bytes, bits);
		}

		template <typename T>
		static std::string StringifyValue(const void* obj)
		{
			return TypeInfoTraits<T>::Stringify(*static_cast<const T*>(obj));
		}

		std::string_view _displayName;
		std::span<const Field> _fields;
		std::string (*_stringifyThunk)(const void*);
	};
}
