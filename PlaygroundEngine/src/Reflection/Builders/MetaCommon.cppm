module;

#include <meta>

export module PlaygroundEngine.Reflection:MetaCommon;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :TypeInfo;
import :TypeReference;

import std;

// Shared foundation for the reflection builders. Holds the recursion knot: TypeOfMeta is only
// declared here, so the field/function builders can reference &TypeOfMeta<memberType>() without
// instantiating it. The single definition lives in the :TypeBuilder assembler partition, which is the
// only unit that imports the sub-builders. That keeps the partition import graph acyclic.

namespace PgE::detail
{
	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta();

	template <std::meta::info MetaType>
	consteval TypeReference TypeReferenceTo()
	{
		// Every cross-type reference stored in the metadata (a field's type, a parameter or return type, an
		// annotation's type, an enum's underlying type) is bound the same way: to the address of TypeOfMeta,
		// not a call. Deferring the call to first runtime read is what lets a type name itself or another
		// type still under construction. See TypeReference.
		//
		// Bind to the canonical (dealiased) type so we never instantiate TypeOfMeta on an alias spelling.
		// TypeOfMeta already dealiases internally, so the alias instantiation would only tail-call this one;
		// skipping it also dodges a GCC-16 reflection mangling collision where the alias's argument is
		// dropped from the symbol, making distinct alias instantiations (e.g. underlying_type_t of two enums)
		// resolve to one duplicately-defined symbol.
		return TypeReference{.Resolve = &TypeOfMeta<std::meta::dealias(MetaType)>};
	}

	template <typename T>
	std::string StringifyValue(const void* obj)
	{
		return TypeInfoTraits<T>::Stringify(*static_cast<const T*>(obj));
	}

	consteval std::string_view IdentifierOf(const std::meta::info entity)
	{
		return std::meta::has_identifier(entity) ? std::meta::identifier_of(entity) : std::string_view{};
	}

	consteval std::string_view DisplayStringOf(const std::meta::info entity)
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
