module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:NestedTypesBuilder;

import :MetaCommon;
import :FacetsBuilder;
import :AnnotationsBuilder;
import :NestedTypeInfo;
import :DeclarationInfo;

import std;

// Builds the NestedTypeInfo list for a type: the type members a class declares inside itself (nested classes,
// enums, unions) and its member type-aliases. members_of already excludes the injected class name, so a plain
// is_type filter is the whole of it. See docs/ReflectionInternals.md (nested types).

namespace PgE::detail
{
	consteval std::vector<std::meta::info> GetNestedTypes(const std::meta::info type)
	{
		std::vector<std::meta::info> nested;
		for (const std::meta::info member : std::meta::members_of(type, std::meta::access_context::unchecked()))
		{
			// An anonymous nested union or struct (the type of `struct { int a; } field;`) is a type member with
			// no identifier: nothing can name it, a projection cannot mirror it, and it is already reachable
			// through the field that uses it. Excluding it also keeps a lookup by empty identifier from matching.
			if (std::meta::is_type(member) && std::meta::has_identifier(member))
			{
				nested.push_back(member);
			}
		}
		return nested;
	}

	template <std::meta::info MetaMember>
	consteval NestedTypeInfo MakeNestedType()
	{
		// A member is an alias when dealiasing changes it (using ValueType = int), a real nested type when it
		// does not (struct Config). The reference is to the dealiased type, so an alias resolves to what it names.
		constexpr bool isAlias = std::meta::dealias(MetaMember) != MetaMember;
		return NestedTypeInfo(TypeReferenceTo<std::meta::dealias(MetaMember)>(), IdentifierOf(MetaMember), DisplayStringOf(MetaMember),
							  ScopePathOf<MetaMember>(), AccessOf(MetaMember), isAlias, MakeAnnotations<MetaMember>());
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval auto MakeNestedTypeArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(GetNestedTypes(MetaType));
		return std::array<NestedTypeInfo, sizeof...(I)>{MakeNestedType<members[I]>()...};
	}

	template <std::meta::info MetaType>
	consteval auto MakeNestedTypesFromType()
	{
		if constexpr (ProvidesSupersedingFacet<MetaType>())
		{
			return std::array<NestedTypeInfo, 0>{};
		}
		else if constexpr (IsClassOrUnion(MetaType))
		{
			constexpr auto count = std::define_static_array(GetNestedTypes(MetaType)).size();
			return MakeNestedTypeArray<MetaType>(std::make_index_sequence<count>{});
		}
		else
		{
			return std::array<NestedTypeInfo, 0>{};
		}
	}
}
