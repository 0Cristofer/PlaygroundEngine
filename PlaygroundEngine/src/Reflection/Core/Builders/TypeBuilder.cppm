module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:TypeBuilder;

import :TypeInfoTraits;

export import :MetaCommon;
export import :AnnotationsBuilder;
export import :ArgumentBinding;
export import :FacetsBuilder;
export import :FieldsBuilder;
export import :StaticFieldsBuilder;
export import :FunctionsBuilder;
export import :OperatorsBuilder;
export import :ConversionsBuilder;
export import :BasesBuilder;
export import :ConstructorsBuilder;
export import :DestructorBuilder;
export import :NestedTypesBuilder;
export import :TemplateBuilder;
export import :FunctionSignatureBuilder;
export import :MemberPointerBuilder;
export import :TraitsBuilder;
import :TypeInfo;
import :Facets;

import std;

// Builds a type's runtime TypeInfo by orchestrating the per-kind builders, and holds the sole definition
// of the TypeMetaOf recursion knot (declared in :MetaCommon). See docs/ReflectionInternals.md (the
// recursion knot, the builder pipeline).

namespace PgE::detail
{
	template <std::meta::info MetaType>
	consteval TypeReference MakeInnerType()
	{
		// Peel exactly one shape, so a walk chains: const Foo* yields const Foo, which yields Foo. cv is
		// peeled before the others, since a const pointer is a cv node whose inner is the pointer.
		if constexpr (std::meta::is_const_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_const(MetaType)>();
		}
		else if constexpr (std::meta::is_volatile_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_volatile(MetaType)>();
		}
		else if constexpr (std::meta::is_pointer_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_pointer(MetaType)>();
		}
		else if constexpr (std::meta::is_reference_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_reference(MetaType)>();
		}
		else if constexpr (std::meta::is_array_type(MetaType))
		{
			return TypeReferenceTo<std::meta::remove_extent(MetaType)>();
		}
		else
		{
			return TypeReference{};
		}
	}

	// Whether the demand pass has run for this type. False on the metadata walk (the four lists are empty and
	// pending); set true by MaterializeTypeOps once TypeOf<T> names the type. TypeInfo::AreOpsMaterialized reads
	// it so a caller can tell an empty GetFunctions() (not yet materialized) from a type that genuinely has none.
	template <std::meta::info MetaType>
	inline constinit bool GOpsMaterialized = false;

	template <std::meta::info MetaType>
	consteval TypeInfo MakeType()
	{
		using T = [:MetaType:];
		constexpr std::string_view identifier = TypeIdentifierOf(MetaType);
		constexpr std::string_view displayName = DisplayStringOf(MetaType);

		static constexpr auto Fields = MakeFieldsFromType<MetaType>();
		static constexpr auto StaticFields = MakeStaticFieldsFromType<MetaType>();
		static constexpr auto Bases = MakeBasesFromType<MetaType>();
		static constexpr auto NestedTypes = MakeNestedTypesFromType<MetaType>();

		static constexpr auto Annotations = MakeAnnotations<MetaType>();

		constexpr TypeTraits traits = MakeTraits<MetaType>();

		static constexpr auto Facets = MakeFacetsFromType<MetaType>();

		// The function, operator, conversion, and constructor lists are built only at demand, so the metadata
		// walk never reifies a member (which would instantiate its body). TypeInfo points at empty view globals
		// that MaterializeTypeOps publishes. The other lists are immutable and built here. See the two tiers.
		if constexpr (Fields.empty() && std::is_object_v<T> && std::meta::is_complete_type(MetaType))
		{
			// A fieldless object is a leaf: it stringifies through its (total) trait, so it always gets a thunk;
			// the guard excludes void and an incomplete type, neither of which StringifyValue can dereference.
			return TypeInfo(identifier, displayName, ScopePathOf<MetaType>(), Annotations, traits, Facets, &GFunctionsView<MetaType>, &GOperatorsView<MetaType>,
							&GConversionsView<MetaType>, Fields, StaticFields, Bases, &GConstructorsView<MetaType>, &GDestructor<MetaType>, NestedTypes,
							MakeTemplate<MetaType>(), MakeTemplateArguments<MetaType>(), MakeInnerType<MetaType>(), MakeFunctionSignature<MetaType>(),
							MakeMemberPointer<MetaType>(), &StringifyValue<T>, &GOpsMaterialized<MetaType>);
		}
		else
		{
			return TypeInfo(identifier, displayName, ScopePathOf<MetaType>(), Annotations, traits, Facets, &GFunctionsView<MetaType>, &GOperatorsView<MetaType>,
							&GConversionsView<MetaType>, Fields, StaticFields, Bases, &GConstructorsView<MetaType>, &GDestructor<MetaType>, NestedTypes,
							MakeTemplate<MetaType>(), MakeTemplateArguments<MetaType>(), MakeInnerType<MetaType>(), MakeFunctionSignature<MetaType>(),
							MakeMemberPointer<MetaType>(), nullptr, &GOpsMaterialized<MetaType>);
		}
	}

	template <std::meta::info MetaType>
	constexpr const TypeInfo& TypeMetaOf()
	{
		// Key the cached TypeInfo on the canonical (dealiased) type, so every alias resolves to one instance
		// and pointer identity holds. See docs/ReflectionInternals.md (canonical caching and dealiasing).
		if constexpr (std::meta::dealias(MetaType) != MetaType)
		{
			return TypeMetaOf<std::meta::dealias(MetaType)>();
		}
		else
		{
			static constexpr TypeInfo Type = MakeType<MetaType>();
			return Type;
		}
	}

	// The demand upgrade: builds and publishes the function/operator/conversion/constructor lists (with their
	// ops) and the non-trivial destructor. It runs once, at static-init before main, and only for a type named
	// through TypeOf<T>, so a type merely reached as metadata never reifies a member. See the demand ops doc.
	template <std::meta::info MetaType>
	void MaterializeTypeOps()
	{
		// Set unconditionally: once the demand pass runs, the four lists are at their final value, empty or not.
		// A superseded or non-class type publishes nothing, but its empty lists are then settled, not pending.
		GOpsMaterialized<MetaType> = true;

		// A superseded type leaves its function/operator/conversion/constructor views empty (never published),
		// so its members are never reified: std::string is reached as its facet, never as its 150-odd
		// deprecated-laced members.
		if constexpr (IsClassOrUnion(MetaType) && !ProvidesSupersedingFacet<MetaType>())
		{
			MaterializeFunctions<MetaType>();
			MaterializeOperators<MetaType>();
			MaterializeConversions<MetaType>();
			MaterializeConstructors<MetaType>();

			// A non-trivial destructor is materialized here; a trivial one is already set from the metadata
			// build. A superseded type's lifetime is the facet's, so it is left out with the rest.
			FillDestroyer<MetaType>();
		}
	}

	template <std::meta::info MetaType>
	struct TypeOpsUpgrader
	{
		TypeOpsUpgrader()
		{
			MaterializeTypeOps<MetaType>();
		}
	};

	template <std::meta::info MetaType>
	inline const TypeOpsUpgrader<MetaType> GTypeOpsUpgrader{};

	export template <std::meta::info MetaType>
	const TypeInfo& MaterializedTypeOf()
	{
		// Odr-using the upgrader forces its constructor to run at static init: the named type gets its ops, and
		// a type reached only through TypeReferenceTo (which resolves to TypeMetaOf) stays metadata alone.
		(void)&GTypeOpsUpgrader<std::meta::dealias(MetaType)>;

		return TypeMetaOf<MetaType>();
	}
}
