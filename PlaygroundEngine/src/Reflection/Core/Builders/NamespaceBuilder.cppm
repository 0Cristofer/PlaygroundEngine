module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:NamespaceBuilder;

import :MetaCommon;
import :AnnotationsBuilder;
import :NestedTypesBuilder;
import :FunctionsBuilder;
import :StaticFieldsBuilder;
import :NamespaceInfo;
import :NestedTypeInfo;
import :FunctionInfo;
import :StaticFieldInfo;

import std;

// Builds a namespace's runtime NamespaceInfo by sorting its members into the four reflectable kinds and
// naming each one through the same per-entity singleton a caller would reach directly.
// See docs/ReflectionExtraction.md (namespace sweep).

namespace PgE::detail
{
	consteval bool IsReflectableNamespaceType(const std::meta::info member)
	{
		// An unnamed type member cannot be looked up or mirrored by a projection, and it is already reachable
		// through whatever names it. A class template is not a type until instantiated, so it is not one here.
		return std::meta::is_type(member) && std::meta::has_identifier(member) && !std::meta::is_template(member);
	}

	consteval bool IsReflectableNamespaceFunction(const std::meta::info member)
	{
		// Mirrors GetMemberFunctions: an operator or a literal operator is modelled by kind, not by name, and
		// has no free-standing metadata to point at. An uninstantiated template has no signature to reflect.
		if (!std::meta::is_function(member) || std::meta::is_template(member) || std::meta::is_concept(member))
		{
			return false;
		}

		return !std::meta::is_operator_function(member) && !std::meta::is_conversion_function(member) && !std::meta::is_literal_operator(member);
	}

	consteval bool IsReflectableNamespaceVariable(const std::meta::info member)
	{
		// A reference variable is omitted rather than allowed to fail: naming one directly is a static_assert
		// (it crashes GCC 17), but failing a whole namespace over one declaration is far worse than not
		// reporting it. See docs/ReflectionExtraction.md (namespace sweep).
		if (IsReferenceVariable(member))
		{
			return false;
		}

		return std::meta::is_variable(member) && std::meta::has_identifier(member) && !std::meta::is_template(member);
	}

	consteval bool IsReflectableNamespaceScope(const std::meta::info member)
	{
		// is_namespace is true for an alias too, so the alias check comes first: an alias names a namespace that
		// is already reported under its own name, and following it would report the same entity twice.
		if (!std::meta::is_namespace(member) || std::meta::is_namespace_alias(member))
		{
			return false;
		}

		// An anonymous namespace has no identifier to key on. Note this is a naming rule, not a linkage one:
		// an internal-linkage function or variable is still swept. See docs/ReflectionExtraction.md (gaps).
		return std::meta::has_identifier(member);
	}

	consteval std::vector<std::meta::info> GetNamespaceMembers(const std::meta::info scope, bool (*matches)(std::meta::info))
	{
		std::vector<std::meta::info> members;
		for (const std::meta::info member : std::meta::members_of(scope, std::meta::access_context::unchecked()))
		{
			if (matches(member))
			{
				members.push_back(member);
			}
		}
		return members;
	}

	export template <std::meta::info MetaNamespace>
	constexpr const NamespaceInfo& NamespaceMetaOf();

	template <std::meta::info MetaNamespace, std::size_t... I>
	consteval std::array<NestedTypeInfo, sizeof...(I)> MakeNamespaceTypeArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceType));
		return std::array<NestedTypeInfo, sizeof...(I)>{MakeNestedType<members[I]>()...};
	}

	template <std::meta::info MetaNamespace, std::size_t... I>
	consteval std::array<const FunctionInfo*, sizeof...(I)> MakeNamespaceFunctionArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceFunction));
		return std::array<const FunctionInfo*, sizeof...(I)>{&FunctionMetaOf<members[I]>()...};
	}

	template <std::meta::info MetaNamespace, std::size_t... I>
	consteval std::array<const StaticFieldInfo*, sizeof...(I)> MakeNamespaceVariableArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceVariable));
		return std::array<const StaticFieldInfo*, sizeof...(I)>{&VariableMetaOf<members[I]>()...};
	}

	template <std::meta::info MetaNamespace, std::size_t... I>
	consteval std::array<const NamespaceInfo*, sizeof...(I)> MakeNamespaceScopeArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto members = std::define_static_array(GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceScope));
		return std::array<const NamespaceInfo*, sizeof...(I)>{&NamespaceMetaOf<members[I]>()...};
	}

	template <std::meta::info MetaNamespace>
	consteval NamespaceInfo MakeNamespace()
	{
		constexpr auto typeCount = GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceType).size();
		constexpr auto functionCount = GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceFunction).size();
		constexpr auto variableCount = GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceVariable).size();
		constexpr auto scopeCount = GetNamespaceMembers(MetaNamespace, &IsReflectableNamespaceScope).size();

		static constexpr auto Types = MakeNamespaceTypeArray<MetaNamespace>(std::make_index_sequence<typeCount>{});
		static constexpr auto Functions = MakeNamespaceFunctionArray<MetaNamespace>(std::make_index_sequence<functionCount>{});
		static constexpr auto Variables = MakeNamespaceVariableArray<MetaNamespace>(std::make_index_sequence<variableCount>{});
		static constexpr auto Namespaces = MakeNamespaceScopeArray<MetaNamespace>(std::make_index_sequence<scopeCount>{});

		static constexpr auto Annotations = MakeAnnotations<MetaNamespace>();

		return NamespaceInfo(IdentifierOf(MetaNamespace), DisplayStringOf(MetaNamespace), ScopePathOf<MetaNamespace>(), Annotations, Types, Functions,
							 Variables, Namespaces);
	}

	export template <std::meta::info MetaNamespace>
	constexpr const NamespaceInfo& NamespaceMetaOf()
	{
		// The rejections must gate the instantiation, not merely diagnose it: a static_assert does not stop the
		// build from going on to instantiate MakeNamespace, and sweeping the global namespace reaches deprecated
		// std entities (hard errors under -Werror) and then segfaults cc1plus. See docs/ReflectionExtraction.md.
		if constexpr (!std::meta::is_namespace(MetaNamespace) || !std::meta::has_parent(MetaNamespace))
		{
			static_assert(std::meta::is_namespace(MetaNamespace), "NamespaceOf reflects a namespace; a type is reached through TypeOf<T>()");
			static_assert(!std::meta::is_namespace(MetaNamespace) || std::meta::has_parent(MetaNamespace),
						  "NamespaceOf cannot sweep the global namespace; reflect a named namespace");

			// Never read: the assert above has already failed the build. It exists so the rejecting branch has
			// a value to return without instantiating MakeNamespace.
			static constexpr NamespaceInfo Rejected({}, {}, {}, {}, {}, {}, {}, {});
			return Rejected;
		}
		// Keyed on the dealiased namespace, so an alias and the namespace it names share one instance and
		// pointer identity holds, exactly as it does for a type alias.
		else if constexpr (std::meta::dealias(MetaNamespace) != MetaNamespace)
		{
			return NamespaceMetaOf<std::meta::dealias(MetaNamespace)>();
		}
		else
		{
			static constexpr NamespaceInfo Namespace = MakeNamespace<MetaNamespace>();
			return Namespace;
		}
	}
}
