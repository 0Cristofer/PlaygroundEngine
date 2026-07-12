module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:MetaCommon;

import :TypeInfoTraits;

import :TypeInfo;
import :TypeReference;

import std;

// Shared foundation for the reflection builders, and the declaration of the TypeOfMeta recursion knot.
// See docs/ReflectionInternals.md (the recursion knot, the facet-authoring toolkit).

namespace PgE::detail
{
	// TypeOfMeta, TypeReferenceTo, IdentifierOf, DisplayStringOf are the exported facet-authoring toolkit;
	// StringifyValue and IsClassOrUnion stay module-internal. See docs/ReflectionInternals.md (Facets).

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

	consteval bool IsClassOrUnion(const std::meta::info type)
	{
		// Both carry reflectable data members and member functions, but is_class_type is false for a
		// union, so the member-walking builders must admit both.
		return std::meta::is_class_type(type) || std::meta::is_union_type(type);
	}
}
