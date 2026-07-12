export module PlaygroundEngine.Reflection.Core:DeclarationInfo;

import :TypeReference;

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

	export class DeclarationInfo
	{
		// The metadata every reflected declaration kind (type, field, function, parameter)
		// shares: its names and its annotations. The identifier is what the author wrote in
		// code and is the query key; it is empty for unnamed entities (fundamental types,
		// unnamed parameters). The display name is implementation-defined diagnostic text
		// for humans, always present, never a key.

	public:
		constexpr DeclarationInfo(const std::string_view identifier,
								  const std::string_view displayName,
								  const std::span<const AnnotationInfo> annotations)
			: _identifier(identifier), _displayName(displayName), _annotations(annotations)
		{}

		std::string_view GetIdentifier() const { return _identifier; }

		std::string_view GetDisplayName() const { return _displayName; }

		std::span<const AnnotationInfo> GetAnnotations() const { return _annotations; }

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
		std::string_view _identifier;
		std::string_view _displayName;
		std::span<const AnnotationInfo> _annotations;
	};
}
