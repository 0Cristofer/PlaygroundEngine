export module PlaygroundTests.SnapshotHarness;

import PlaygroundEngine.Reflection;

import std;

namespace PgE::Harness
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

		std::string_view KindName(const TypeKind kind)
		{
			switch (kind)
			{
			case TypeKind::Void: return "Void";
			case TypeKind::NullPointer: return "NullPointer";
			case TypeKind::Integral: return "Integral";
			case TypeKind::FloatingPoint: return "FloatingPoint";
			case TypeKind::Enum: return "Enum";
			case TypeKind::Union: return "Union";
			case TypeKind::Class: return "Class";
			case TypeKind::Array: return "Array";
			case TypeKind::Pointer: return "Pointer";
			case TypeKind::MemberObjectPointer: return "MemberObjectPointer";
			case TypeKind::MemberFunctionPointer: return "MemberFunctionPointer";
			case TypeKind::Function: return "Function";
			case TypeKind::LValueReference: return "LValueReference";
			case TypeKind::RValueReference: return "RValueReference";
			case TypeKind::Other: return "Other";
			}

			return "Other";
		}
	}

	// A reflected type's machine-facing shape as stable text, split into an identity section (names,
	// kinds, field order, the surface) and a layout section (the ABI numbers). The split is deliberate:
	// a compiler upgrade that only shifts padding touches the layout region alone, so a snapshot diff
	// shows at a glance whether anything structural changed. Identifiers are preferred over display
	// names because they are stable; fundamentals have no identifier and fall back to the display name,
	// which is stable for them.
	export template <typename T>
	std::string DescribeType()
	{
		const TypeInfo& type = TypeOf<T>();
		std::string out;
		const auto sink = std::back_inserter(out);

		std::format_to(sink, "type {}\n", detail::Label(type));
		std::format_to(sink, "kind {}\n", detail::KindName(type.GetKind()));

		out += "identity\n";
		for (const FieldInfo& field : type.GetFields())
			std::format_to(sink, "  field {} : {}\n", field.GetIdentifier(), detail::Label(field.GetTypeInfo()));
		for (const FunctionInfo& function : type.GetFunctions())
			std::format_to(sink, "  function {}\n", function.GetIdentifier());
		for (const FacetEntry& facet : type.GetFacets())
			std::format_to(sink, "  facet {}\n", detail::Label(facet.Key.Get()));
		for (const AnnotationInfo& annotation : type.GetAnnotations())
			std::format_to(sink, "  annotation {}\n", detail::Label(annotation.Type.Get()));

		const TypeTraits& traits = type.GetTraits();
		out += "layout\n";
		std::format_to(sink, "  size {}\n", traits.Size);
		std::format_to(sink, "  alignment {}\n", traits.Alignment);
		std::format_to(sink, "  trivially_copyable {}\n", detail::Bool(traits.IsTriviallyCopyable));
		std::format_to(sink, "  standard_layout {}\n", detail::Bool(traits.IsStandardLayout));
		std::format_to(sink, "  unique_object_representations {}\n", detail::Bool(traits.HasUniqueObjectRepresentations));
		for (const FieldInfo& field : type.GetFields())
			std::format_to(sink, "  field {} @ {}\n", field.GetIdentifier(), field.GetByteOffset());

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
