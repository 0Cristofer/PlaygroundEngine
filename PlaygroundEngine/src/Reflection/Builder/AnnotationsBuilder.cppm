module;

#include <meta>

export module PlaygroundEngine.Reflection:AnnotationsBuilder;

import :MetaCommon;
import :DeclarationInfo;

import std;

// Materializes an entity's [[=annotation]] values into program-lifetime storage and packs them as a
// span of AnnotationInfo for any declaration kind (type, field, function, parameter).

namespace PgE::detail
{
	template <std::meta::info Annotation>
	struct AnnotationStorage
	{
		// Materializes one annotation value into static storage, keyed by its reflection so
		// each distinct annotation gets its own constant with a stable, program-lifetime
		// address. The split alias keeps the spliced type out of a template argument
		// (GCC -Wtemplate-body).

		using Declared = [:std::meta::type_of(Annotation):];
		using Type = std::remove_cvref_t<Declared>;
		static constexpr Type Value = std::meta::extract<Type>(Annotation);
	};

	template <std::meta::info Annotation>
	consteval AnnotationInfo MakeAnnotation()
	{
		// The type tag is &TypeOfMeta<...>(), the same instance a caller reaches through
		// &TypeOf<A>(), so a runtime GetAnnotation<A>() matches by pointer identity.
		return AnnotationInfo{
			.Type = &TypeOfMeta<std::meta::remove_cvref(std::meta::type_of(Annotation))>(),
			.Value = &AnnotationStorage<Annotation>::Value,
		};
	}

	consteval std::vector<std::meta::info> GetAnnotationList(const std::meta::info entity)
	{
		std::vector<std::meta::info> annotations;
		for (const std::meta::info annotation : std::meta::annotations_of(entity))
			annotations.push_back(annotation);
		return annotations;
	}

	template <std::meta::info Entity, std::size_t... I>
	consteval auto MakeAnnotationArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto annotations = std::define_static_array(GetAnnotationList(Entity));
		return std::array<AnnotationInfo, sizeof...(I)>{MakeAnnotation<annotations[I]>()...};
	}

	template <std::meta::info Entity>
	constexpr std::span<const AnnotationInfo> MakeAnnotations()
	{
		constexpr auto annotationCount = GetAnnotationList(Entity).size();
		static constexpr auto Annotations = MakeAnnotationArray<Entity>(std::make_index_sequence<annotationCount>{});
		return Annotations;
	}
}
