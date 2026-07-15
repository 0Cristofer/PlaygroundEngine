export module PlaygroundEngine.Reflection.Core:TemplateInfo;

import :TypeReference;
import :EntityInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export class TemplateInfo : public EntityInfo
	{
		// The primary template a type was instantiated from. A template is not a type, so it cannot be a
		// TypeReference: naming it is all that is possible. It is an EntityInfo rather than a DeclarationInfo
		// because a template cannot be annotated. See docs/ReflectionInternals.md (templates).

	public:
		constexpr TemplateInfo(const std::string_view identifier,
							   const std::string_view displayName,
							   const std::span<const std::string_view> scopePath)
			: EntityInfo(identifier, displayName, scopePath)
		{}
	};

	export enum class TemplateArgumentKind : std::uint8_t
	{
		Type,
		Value,
		Template,
	};

	export struct TemplateArgumentInfo
	{
		// A template argument has three kinds, not two: a template template argument (Holder<Grid, Foo>) is
		// neither a type nor a value. See docs/ReflectionInternals.md (template arguments) for why a
		// defaulted argument is indistinguishable from a written one.

		TemplateArgumentKind Kind = TemplateArgumentKind::Type;

		// The type argument, or the non-type argument's own type. Unset for a Template argument.
		TypeReference Type;

		const void* Value = nullptr;
		const TemplateInfo* Template = nullptr;
	};
}
