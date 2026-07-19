export module PlaygroundEngine.Reflection.Core:DeclarationInfo;

import :TypeReference;
import :EntityInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export struct AnnotationInfo
	{
		TypeReference Type;
		const void* Value = nullptr;
	};

	export class DeclarationInfo : public EntityInfo
	{
		// A named entity the language lets you annotate: a type, type alias, variable, function, function
		// parameter, namespace, enumerator, base class, or non-static data member. That list is the language's,
		// not ours, which is why a template is an EntityInfo and not this. See docs/ReflectionInternals.md.

	public:
		constexpr DeclarationInfo(const std::string_view identifier,
								  const std::string_view displayName,
								  const std::span<const std::string_view> scopePath,
								  const std::span<const AnnotationInfo> annotations)
			: EntityInfo(identifier, displayName, scopePath), _annotations(annotations)
		{}

		std::span<const AnnotationInfo> GetAnnotations() const
		{
			return _annotations;
		}

		template <typename A>
		std::vector<const std::remove_cvref_t<A>*> GetAnnotations() const
		{
			using AnnotationType = std::remove_cvref_t<A>;

			std::vector<const AnnotationType*> annotations;
			for (const auto& [Type, Value] : _annotations)
			{
				if (&Type.Get() == &TypeOf<AnnotationType>())
				{
					annotations.push_back(static_cast<const AnnotationType*>(Value));
				}
			}
			return annotations;
		}

		template <typename A>
		bool HasAnnotation() const
		{
			for (const auto& [Type, Value] : _annotations)
			{
				if (&Type.Get() == &TypeOf<std::remove_cvref_t<A>>())
				{
					return true;
				}
			}
			return false;
		}

	private:
		std::span<const AnnotationInfo> _annotations;
	};
}
