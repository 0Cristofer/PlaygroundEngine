module;

#include <meta>

export module PlaygroundEngine.Reflection.Core;

export import :TypeInfoTraits;
export import :TypeReference;
export import :TypedRef;
export import :EntityInfo;
export import :DeclarationInfo;
export import :ParameterInfo;
export import :FieldInfo;
export import :StaticFieldInfo;
export import :FunctionInfo;
export import :OperatorInfo;
export import :ConversionInfo;
export import :BaseInfo;
export import :ConstructorInfo;
export import :DestructorInfo;
export import :NestedTypeInfo;
export import :NamespaceInfo;
export import :TemplateInfo;
export import :FunctionSignatureInfo;
export import :MemberPointerInfo;
export import :TypeInfo;
export import :Facets;
export import :TypeBuilder;
export import :NamespaceBuilder;

import std;

// The reflection core: the type model (the ...Info types), the facet-table mechanism, the builders that
// assemble a TypeInfo, and rendering. It names no concrete facet, so a facet is an extension on top.
// See docs/ReflectionInternals.md (module layering, rendering).

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

	// The one entry point that asks a caller to write ^^, because it has to: a namespace is not a type, a
	// value, or a template, so it cannot be a template argument in any other form.
	export template <std::meta::info MetaNamespace>
	constexpr const NamespaceInfo& NamespaceOf()
	{
		return detail::NamespaceOfMeta<MetaNamespace>();
	}

	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj);

	export template <typename T>
	std::string ToString(const T& value)
	{
		return ObjectToString(TypeOf(value), &value);
	}
}
