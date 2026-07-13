export module PlaygroundTests.SnapshotHarness;

import PlaygroundEngine.Reflection;

import std;

namespace PgE::Snapshot
{
	namespace detail
	{
		std::string_view Label(const TypeInfo& type)
		{
			// The identifier is what the author wrote (stable); the display name is
			// implementation-defined but stable for fundamentals, which have no identifier.
			return type.GetIdentifier().empty() ? type.GetDisplayName() : type.GetIdentifier();
		}

		std::string_view Bool(const bool value)
		{
			return value ? "true" : "false";
		}
	}

	// A reflected type's machine-facing shape as stable text: an identity section (names, kinds, field
	// order) and a layout section (the ABI numbers), kept separate so a padding-only change stays
	// isolated. See docs/CorrectnessAndStandards.md. Identifiers are preferred over display names for stability.
	export template <typename T>
	std::string DescribeType()
	{
		const TypeInfo& type = TypeOf<T>();
		std::string out;
		const auto sink = std::back_inserter(out);

		std::format_to(sink, "type {}\n", detail::Label(type));
		std::format_to(sink, "kind {}\n", ToString(type.GetKind()));

		out += "identity\n";
		for (const FieldInfo& field : type.GetFields())
		{
			std::format_to(sink, "  field {} : {}\n", field.GetIdentifier(), detail::Label(field.GetTypeInfo()));
		}
		for (const FunctionInfo& function : type.GetFunctions())
		{
			std::format_to(sink, "  function {}\n", function.GetIdentifier());
		}
		for (const FacetEntry& facet : type.GetFacets())
		{
			std::format_to(sink, "  facet {}\n", detail::Label(facet.Key.Get()));
		}
		for (const AnnotationInfo& annotation : type.GetAnnotations())
		{
			std::format_to(sink, "  annotation {}\n", detail::Label(annotation.Type.Get()));
		}

		const TypeTraits& traits = type.GetTraits();
		out += "layout\n";
		std::format_to(sink, "  size {}\n", traits.Size);
		std::format_to(sink, "  alignment {}\n", traits.Alignment);
		std::format_to(sink, "  trivially_copyable {}\n", detail::Bool(traits.IsTriviallyCopyable));
		std::format_to(sink, "  standard_layout {}\n", detail::Bool(traits.IsStandardLayout));
		std::format_to(sink, "  unique_object_representations {}\n", detail::Bool(traits.HasUniqueObjectRepresentations));
		for (const FieldInfo& field : type.GetFields())
		{
			std::format_to(sink, "  field {} @ {}\n", field.GetIdentifier(), field.GetByteOffset());
		}

		return out;
	}

	export struct SnapshotResult
	{
		bool Matched = false;
		std::string Detail;
	};

	// Compare `actual` against the committed golden named `name` under the snapshot directory. With the
	// PGE_BLESS environment variable set, the golden is (re)written instead of compared, so an intended
	// change is consciously re-accepted. A missing golden without blessing is a failure, not a silent pass.
	export SnapshotResult CheckSnapshot(std::string_view name, std::string_view actual);
}
