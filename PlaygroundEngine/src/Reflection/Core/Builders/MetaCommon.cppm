module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:MetaCommon;

import :TypeInfoTraits;

import :TypeInfo;
import :TypeReference;
import :DeclarationInfo;

import std;

// Shared foundation for the reflection builders, and the declaration of the TypeOfMeta recursion knot.
// See docs/ReflectionInternals.md (the recursion knot, the facet-authoring toolkit).

namespace PgE::detail
{
	// TypeOfMeta, TypeReferenceTo, IdentifierOf, TypeIdentifierOf, DisplayStringOf, and ScopePathOf are the
	// exported facet-authoring toolkit; the rest stay module-internal. See docs/ReflectionInternals.md (Facets).

	export template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta();

	export template <std::meta::info MetaType>
	consteval TypeReference TypeReferenceTo()
	{
		// Bind to the address of TypeOfMeta on the dealiased type, not a call. See docs/ReflectionInternals.md
		// (TypeReference lazy cross-references, and the GCC 16 mangling-collision workaround the dealias dodges).
		return TypeReference{.Resolve = &TypeOfMeta<std::meta::dealias(MetaType)>};
	}

	template <typename T>
	std::string StringifyValue(const void* obj)
	{
		return TypeInfoTraits<T>::Stringify(*static_cast<const T*>(obj));
	}

	export consteval std::string_view IdentifierOf(const std::meta::info entity)
	{
		return std::meta::has_identifier(entity) ? std::meta::identifier_of(entity) : std::string_view{};
	}

	export consteval std::string_view DisplayStringOf(const std::meta::info entity)
	{
		return std::meta::display_string_of(entity);
	}

	consteval std::string_view FundamentalIdentifierOf(const std::meta::info type)
	{
		// A fundamental type has no identifier, so it is named from structure (kind, size, signedness) rather
		// than spelling: stdlib-neutral, and a non-native provider can describe the same type with no C++ name.
		// See docs/ReflectionInternals.md (structural naming) for the char and long double caveats.
		if (type == ^^void)
		{
			return "void";
		}
		if (type == ^^decltype(nullptr))
		{
			return "nullptr_t";
		}
		if (type == ^^bool)
		{
			return "bool";
		}

		// Named by spelling: size and signedness alone collapse char, signed char, and int8_t, which are
		// distinct types, and char is the string element type StringFacet keys on.
		if (type == ^^char)
		{
			return "char";
		}
		if (type == ^^wchar_t)
		{
			return "wchar_t";
		}
		if (type == ^^char8_t)
		{
			return "char8_t";
		}
		if (type == ^^char16_t)
		{
			return "char16_t";
		}
		if (type == ^^char32_t)
		{
			return "char32_t";
		}

		// Not named by size: x86 stores an 80-bit extended long double in 16 bytes, so a width would
		// claim a binary128 it is not, and would collide with a real one.
		if (type == ^^long double)
		{
			return "longdouble";
		}

		if (std::meta::is_integral_type(type))
		{
			const bool isSigned = std::meta::is_signed_type(type);
			switch (std::meta::size_of(type))
			{
			case 1:
				return isSigned ? "int8" : "uint8";
			case 2:
				return isSigned ? "int16" : "uint16";
			case 4:
				return isSigned ? "int32" : "uint32";
			case 8:
				return isSigned ? "int64" : "uint64";
			case 16:
				return isSigned ? "int128" : "uint128";
			default:
				return {};
			}
		}

		if (std::meta::is_floating_point_type(type))
		{
			switch (std::meta::size_of(type))
			{
			case 2:
				return "float16";
			case 4:
				return "float32";
			case 8:
				return "float64";
			default:
				return {};
			}
		}

		return {};
	}

	export consteval std::string_view TypeIdentifierOf(const std::meta::info type)
	{
		// A cv-qualified type is a decomposition node, not a named one: the consumer renders it as const<inner>,
		// so naming it here would report "int32" for both int and const int.
		if (std::meta::has_identifier(type))
		{
			return std::meta::identifier_of(type);
		}

		if (std::meta::is_const_type(type) || std::meta::is_volatile_type(type))
		{
			return {};
		}

		return FundamentalIdentifierOf(type);
	}

	consteval AccessKind AccessOf(const std::meta::info entity)
	{
		if (std::meta::is_public(entity))
		{
			return AccessKind::Public;
		}
		if (std::meta::is_protected(entity))
		{
			return AccessKind::Protected;
		}
		return AccessKind::Private;
	}

	consteval std::vector<std::meta::info> GetScopeChain(const std::meta::info entity)
	{
		// has_parent is the guard, not an optimization: parent_of throws for an entity that has no parent
		// (a fundamental type, void, a pointer), which would abort the whole build. The walk stops at the
		// first unnamed scope, which is the global namespace.
		std::vector<std::meta::info> parts;

		std::meta::info current = entity;
		while (std::meta::has_parent(current))
		{
			current = std::meta::parent_of(current);
			if (!std::meta::has_identifier(current))
			{
				break;
			}

			parts.push_back(current);
		}

		std::ranges::reverse(parts);
		return parts;
	}

	template <std::meta::info Entity, std::size_t... I>
	consteval auto MakeScopeArray(std::index_sequence<I...>)
	{
		// The chain is collected as reflections, not string_views: define_static_array needs a structural
		// element type and string_view is not one, so the identifiers are read per index instead. They need no
		// freezing of their own: identifier_of already yields static storage, which IdentifierOf relies on too.
		[[maybe_unused]] constexpr auto scopes = std::define_static_array(GetScopeChain(Entity));
		return std::array<std::string_view, sizeof...(I)>{std::meta::identifier_of(scopes[I])...};
	}

	export template <std::meta::info Entity>
	constexpr std::span<const std::string_view> ScopePathOf()
	{
		constexpr auto scopeCount = GetScopeChain(Entity).size();
		static constexpr auto Path = MakeScopeArray<Entity>(std::make_index_sequence<scopeCount>{});
		return Path;
	}

	consteval bool IsClassOrUnion(const std::meta::info type)
	{
		// Both carry reflectable data members and member functions, but is_class_type is false for a
		// union, so the member-walking builders must admit both. A cv-qualified class is excluded: it is a
		// decomposition node. See docs/ReflectionInternals.md (compound types, incomplete types).
		if (std::meta::is_const_type(type) || std::meta::is_volatile_type(type))
		{
			return false;
		}

		// An incomplete type has no members to walk, and every walk throws on one. It is reachable now that a
		// pointer names its pointee, which is the whole point of an opaque handle or a PIMPL.
		if (!std::meta::is_complete_type(type))
		{
			return false;
		}

		return std::meta::is_class_type(type) || std::meta::is_union_type(type);
	}
}
