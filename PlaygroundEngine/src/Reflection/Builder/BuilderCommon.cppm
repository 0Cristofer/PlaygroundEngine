module;

#include <meta>

export module PlaygroundEngine.Reflection:BuilderCommon;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :TypeInfo;

import std;

// Shared foundation for the reflection builders. Holds the recursion knot: TypeOfMeta is only
// declared here, so the field/function builders can reference &TypeOfMeta<memberType>() without
// instantiating it. The single definition lives in the :Builder assembler partition, which is the
// only unit that imports the sub-builders. That keeps the partition import graph acyclic.

namespace PgE::detail
{
	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeOfMeta();

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
}