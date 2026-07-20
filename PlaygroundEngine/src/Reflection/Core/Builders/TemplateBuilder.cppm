module;

#include <meta>

export module PlaygroundEngine.Reflection.Core:TemplateBuilder;

import :MetaCommon;
import :TemplateInfo;
import :DeclarationInfo;

import std;

// Builds the primary-template reference and the template-argument list for a template instance, so
// Grid<int> keeps a recoverable relationship to Grid and to int. See docs/ReflectionInternals.md.

namespace PgE::detail
{
	template <std::meta::info MetaTemplate>
	constexpr const TemplateInfo& TemplateMetaOf()
	{
		// One program-lifetime TemplateInfo per template, so a consumer can compare templates by pointer
		// identity the way it compares types.
		static constexpr TemplateInfo Template(IdentifierOf(MetaTemplate), DisplayStringOf(MetaTemplate), ScopePathOf<MetaTemplate>());
		return Template;
	}

	template <std::meta::info MetaArgument>
	struct TemplateArgumentValue
	{
		// The non-type argument materialized into program-lifetime storage, the same way an annotation's
		// value is, so the erased const void* has a stable address to point at. The alias keeps the spliced
		// type out of a template argument (GCC -Wtemplate-body).
		using Declared = [:std::meta::type_of(MetaArgument):];

		static constexpr Declared Value = std::meta::extract<Declared>(MetaArgument);
	};

	template <std::meta::info MetaArgument>
	consteval TemplateArgumentInfo MakeTemplateArgument()
	{
		if constexpr (std::meta::is_type(MetaArgument))
		{
			return TemplateArgumentInfo{
				.Kind = TemplateArgumentKind::Type, .Type = TypeReferenceTo<MetaArgument>(), .Value = nullptr, .Template = nullptr};
		}
		else if constexpr (std::meta::is_value(MetaArgument))
		{
			return TemplateArgumentInfo{.Kind = TemplateArgumentKind::Value,
										.Type = TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaArgument))>(),
										.Value = &TemplateArgumentValue<MetaArgument>::Value,
										.Template = nullptr};
		}
		else if constexpr (std::meta::is_object(MetaArgument))
		{
			// A reference parameter (template <int& R>) names an **object** rather than carrying a value, so
			// there is nothing to materialize: the argument is that object, and its address is the erasure.
			// Reading it as a value is not a constant expression, which is why it cannot share the branch above.
			return TemplateArgumentInfo{.Kind = TemplateArgumentKind::Value,
										.Type = TypeReferenceTo<std::meta::remove_cvref(std::meta::type_of(MetaArgument))>(),
										.Value = std::addressof([:MetaArgument:]),
										.Template = nullptr};
		}
		else
		{
			// Neither a type nor a value: a template template argument, the third kind.
			return TemplateArgumentInfo{
				.Kind = TemplateArgumentKind::Template, .Type = {}, .Value = nullptr, .Template = &TemplateMetaOf<MetaArgument>()};
		}
	}

	template <std::meta::info MetaType, std::size_t... I>
	consteval std::array<TemplateArgumentInfo, sizeof...(I)> MakeTemplateArgumentArray(std::index_sequence<I...>)
	{
		[[maybe_unused]] constexpr auto arguments = std::define_static_array(std::meta::template_arguments_of(MetaType));
		return std::array<TemplateArgumentInfo, sizeof...(I)>{MakeTemplateArgument<arguments[I]>()...};
	}

	template <std::meta::info MetaType>
	constexpr std::span<const TemplateArgumentInfo> MakeTemplateArguments()
	{
		// template_arguments_of throws for a type that is not an instance, so has_template_arguments is a
		// guard, not a filter.
		if constexpr (std::meta::has_template_arguments(MetaType))
		{
			constexpr auto count = std::define_static_array(std::meta::template_arguments_of(MetaType)).size();
			static constexpr auto Arguments = MakeTemplateArgumentArray<MetaType>(std::make_index_sequence<count>{});
			return Arguments;
		}
		else
		{
			return {};
		}
	}

	template <std::meta::info MetaType>
	consteval const TemplateInfo* MakeTemplate()
	{
		if constexpr (std::meta::has_template_arguments(MetaType))
		{
			// A partial specialization reports the primary template, which is the wanted behavior: Spec<int*>
			// yields Spec.
			return &TemplateMetaOf<std::meta::template_of(MetaType)>();
		}
		else
		{
			return nullptr;
		}
	}
}
