export module PlaygroundEngine.Reflection;

export import :TypeInfo;
export import :FieldInfo;
export import :FunctionInfo;
export import :TypedRef;
export import :TypeReference;
export import :DeclarationInfo;
export import :EnumerationFacet;
export import :Facets;
export import :StringFacet;
export import :SequenceFacet;
export import :TypeBuilder;

import std;

namespace PgE
{
	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj);

	export template <typename T>
	constexpr const TypeInfo& TypeOf()
	{
		return detail::TypeOfMeta<^^T>();
	}

	export template <typename T>
	constexpr const TypeInfo& TypeOf(const T&)
	{
		return TypeOf<T>();
	}

	export template <typename T>
	std::string ToString(const T& value)
	{
		return ObjectToString(TypeOf(value), &value);
	}

	// Typed sugar over the enumeration facet, for callers who know the enum type. EnumToName has no answer
	// for a value no enumerator names (a flag combination, or a static_cast in-range value); ToString is
	// the never-fails rendering that falls back to the number. The single enum-to-uint64 cast is the one
	// typed-to-erased step the erased FindByValue cannot do for us, since it has no static type to work from.

	export template <typename Enum>
		requires std::is_enum_v<Enum>
	std::optional<std::string_view> EnumToName(const Enum value)
	{
		const TypeInfo& type = TypeOf<Enum>();

		if (const EnumeratorInfo* enumerator = type.GetFacet<EnumerationFacet>()->FindByValue(static_cast<std::uint64_t>(value)))
		{
			return enumerator->GetIdentifier();
		}

		return std::nullopt;
	}

	export template <typename Enum>
		requires std::is_enum_v<Enum>
	std::optional<Enum> EnumFromName(const std::string_view identifier)
	{
		const TypeInfo& type = TypeOf<Enum>();

		if (const EnumeratorInfo* enumerator = type.GetFacet<EnumerationFacet>()->FindByIdentifier(identifier))
		{
			return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(enumerator->GetValue()));
		}

		return std::nullopt;
	}
}
