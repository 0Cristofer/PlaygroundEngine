export module PlaygroundEngine.Reflection:Annotation;

import std;

namespace PgE
{
	export class TypeInfo;

	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export struct AnnotationInfo
	{
		const TypeInfo* Type = nullptr;
		const void* Value = nullptr;
	};

	export class Annotated
	{
	public:
		constexpr explicit Annotated(const std::span<const AnnotationInfo> annotations) :
			_annotations(annotations)
		{
		}

		std::span<const AnnotationInfo> GetAnnotations() const { return _annotations; }

		template <typename A>
		std::vector<const std::remove_cvref_t<A>*> GetAnnotations() const
		{
			using AnnotationType = std::remove_cvref_t<A>;

			std::vector<const AnnotationType*> annotations;
			for (const auto& [Type, Value] : _annotations)
				if (Type == &TypeOf<AnnotationType>())
					annotations.push_back(static_cast<const AnnotationType*>(Value));
			return annotations;
		}

		template <typename A>
		bool HasAnnotation() const
		{
			for (const auto& [Type, Value] : _annotations)
				if (Type == &TypeOf<std::remove_cvref_t<A>>())
					return true;
			return false;
		}

	private:
		std::span<const AnnotationInfo> _annotations;
	};
}
