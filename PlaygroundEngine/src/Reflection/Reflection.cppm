export module PlaygroundEngine.Reflection;

export import PlaygroundEngine.Reflection.Core;
export import PlaygroundEngine.Reflection.Builtins;
export import PlaygroundEngine.Reflection.Contracts;

import std;

// The umbrella: the whole package a consumer imports. It re-exports the core and the built-in facets so that
// every first-party binding is reachable at the point a type is reflected (a facet not imported would fall
// back to structural reflection). Rendering (ObjectToString/ToString) now lives in the core, since it names
// no facet; what remains here is the facet-consuming sugar that does (EnumToName/EnumFromName read the
// enumeration facet), which cannot live in the core.

namespace PgE
{
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
