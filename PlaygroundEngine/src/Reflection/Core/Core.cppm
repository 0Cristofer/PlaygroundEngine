export module PlaygroundEngine.Reflection.Core;

export import :TypeInfoTraits;
export import :TypeReference;
export import :TypedRef;
export import :DeclarationInfo;
export import :FieldInfo;
export import :FunctionInfo;
export import :TypeInfo;
export import :Facets;
export import :TypeBuilder;

import std;

// The reflection core: the type model (the ...Info types), the facet table mechanism (FacetEntry, the
// generic MakeFacetsFromType), and the builders that assemble a TypeInfo. It names no concrete facet, so a
// facet (built-in or user) is an extension layered on top. TypeOf is the entry point; it lives here because
// TypeInfo::GetFacet reflects the facet type through it, so the core cannot be built without it.
//
// Rendering (ObjectToString/ToString) is core too: it drives a type's stringify thunk, or walks its fields
// when there is none. It names no facet either. A facet renders itself through the thunk its TypeInfoTraits
// installs (which may recurse back here for nested content), so there is no per-facet case in the renderer,
// and any type extends its own rendering the same way. That is why ObjectToString can live in the core.

namespace PgE
{
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

	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj);

	export template <typename T>
	std::string ToString(const T& value)
	{
		return ObjectToString(TypeOf(value), &value);
	}
}
