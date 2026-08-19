module PlaygroundEngine.DebugUi;

import :DebugPanelDrawer;

import imgui;

import PlaygroundEngine.Reflection;

import std;

namespace
{
	constexpr float DragSpeed = 0.01f;

	// The name column starts narrower than the value column, and stays draggable from there.
	constexpr float NameColumnWeight = 0.35f;
	constexpr float ValueColumnWeight = 0.65f;

	template <typename Primitive>
	consteval ImGuiDataType ImGuiDataTypeOf()
	{
		if constexpr (std::is_same_v<Primitive, float>)
		{
			return ImGuiDataType_Float;
		}
		else if constexpr (std::is_same_v<Primitive, double>)
		{
			return ImGuiDataType_Double;
		}
		else if constexpr (std::is_signed_v<Primitive>)
		{
			if constexpr (sizeof(Primitive) == 1)
			{
				return ImGuiDataType_S8;
			}
			else if constexpr (sizeof(Primitive) == 2)
			{
				return ImGuiDataType_S16;
			}
			else if constexpr (sizeof(Primitive) == 4)
			{
				return ImGuiDataType_S32;
			}
			else
			{
				return ImGuiDataType_S64;
			}
		}
		else if constexpr (sizeof(Primitive) == 1)
		{
			return ImGuiDataType_U8;
		}
		else if constexpr (sizeof(Primitive) == 2)
		{
			return ImGuiDataType_U16;
		}
		else if constexpr (sizeof(Primitive) == 4)
		{
			return ImGuiDataType_U32;
		}
		else
		{
			return ImGuiDataType_U64;
		}
	}

	// ImGui draws a widget's own label to its right, so a row writes the name into the first column and
	// gives the widget a hidden one. The id comes from the pushed label, since every hidden widget spells
	// its own the same way.
	void BeginLeafRow(const char* label)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::PushID(label);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-std::numeric_limits<float>::min());
	}

	void EndLeafRow()
	{
		ImGui::PopID();
	}

	// A subobject owns the name column with its tree node, and leaves the value column to the caller for a
	// summary. Children are further rows of the same table, so one nesting level indents rather than
	// starting a table of its own and realigning everything below it.
	bool BeginTreeRow(const char* label)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_SpanAvailWidth);

		ImGui::TableNextColumn();
		return open;
	}

	template <typename Primitive>
	bool TryDrawPrimitive(const char* label, const PgE::TypedRef& ref)
	{
		if (ref.Type != &PgE::TypeMetaOf<Primitive>())
		{
			return false;
		}

		BeginLeafRow(label);
		ImGui::BeginDisabled(ref.IsConst);
		if constexpr (std::is_same_v<Primitive, bool>)
		{
			ImGui::Checkbox("##value", static_cast<bool*>(ref.Data));
		}
		else
		{
			ImGui::DragScalar("##value", ImGuiDataTypeOf<Primitive>(), ref.Data, DragSpeed);
		}
		ImGui::EndDisabled();
		EndLeafRow();

		return true;
	}

	template <typename... Types>
	struct PrimitiveTypes
	{
		// The fold short-circuits, so the first type whose identity matches owns the row and the rest are
		// never called. A false result is the "not a primitive" answer the caller dispatches on.
		static bool TryDraw(const char* label, const PgE::TypedRef& ref)
		{
			return (TryDrawPrimitive<Types>(label, ref) || ...);
		}
	};

	using DrawablePrimitives = PrimitiveTypes<bool,
											  float,
											  double,
											  std::int8_t,
											  std::uint8_t,
											  std::int16_t,
											  std::uint16_t,
											  std::int32_t,
											  std::uint32_t,
											  std::int64_t,
											  std::uint64_t,
											  long long,
											  unsigned long long>;

	void DrawValue(const char* label, const PgE::TypedRef& ref);

	void DrawDisabledText(const char* label, const char* text)
	{
		BeginLeafRow(label);
		ImGui::BeginDisabled();
		ImGui::TextUnformatted(text);
		ImGui::EndDisabled();
		EndLeafRow();
	}

	// Answers whether it drew anything
	bool DrawAnnotatedFields(const PgE::TypedRef& object)
	{
		bool drewAnnotatedField = false;

		// GetFields() is direct members only, so an inherited annotated field is reached by walking the bases
		for (const PgE::BaseInfo& base : object.Type->GetBases())
		{
			const PgE::TypeInfo& baseType = base.GetTypeInfo();

			// A base's fields are siblings of the derived type's here, so a shadowed name would otherwise
			// hand two rows the same widget id and make them share edit state.
			ImGui::PushID(&baseType);

			if (baseType.GetFacets().empty())
			{
				// |=, not ||: every base has to be drawn, and || would stop calling once one of them drew.
				drewAnnotatedField |= DrawAnnotatedFields(base.Upcast(object));
			}
			else
			{
				// A facet supersedes the base's structure (a std::string base publishes no fields), so it
				// draws through the facet as one named row rather than as an empty node. The display name,
				// since a specialization has no identifier of its own.
				DrawValue(baseType.GetDisplayName().data(), base.Upcast(object));
				drewAnnotatedField = true;
			}

			ImGui::PopID();
		}

		for (const PgE::FieldInfo& field : object.Type->GetFields())
		{
			if (!field.HasAnnotation<PgE::DrawDebug>())
			{
				continue;
			}

			drewAnnotatedField = true;
			const char* label = field.GetIdentifier().data();

			const auto ref = field.GetRef(object);
			if (!ref)
			{
				// A bitfield has no address, so there is no borrow to edit in place. Any other reason is the
				// walk's own fault and says so rather than posing as one.
				DrawDisabledText(label, ref.error().Reason == PgE::FieldError::NotAddressable ? "<not addressable>" : "<unreadable>");
				continue;
			}

			DrawValue(label, *ref);
		}

		return drewAnnotatedField;
	}

	void DrawFields(const PgE::TypedRef& object)
	{
		if (!DrawAnnotatedFields(object))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("<no annotated fields>");
		}
	}

	void DrawEnumeration(const char* label, const PgE::EnumerationFacet& enumeration, const PgE::TypedRef& ref)
	{
		const std::uint64_t current = enumeration.Value(ref);
		const PgE::EnumeratorInfo* selected = enumeration.FindByValue(current);

		BeginLeafRow(label);
		ImGui::BeginDisabled(ref.IsConst);
		if (ImGui::BeginCombo("##value", selected != nullptr ? selected->GetIdentifier().data() : "<unnamed>"))
		{
			for (const PgE::EnumeratorInfo& enumerator : enumeration.GetEnumerators())
			{
				if (const bool isSelected = enumerator.GetValue() == current; ImGui::Selectable(enumerator.GetIdentifier().data(), isSelected))
				{
					// Discarded, and it cannot fail: a read-only borrow disables the combo, so no selection reaches here.
					(void)enumeration.Assign(ref, enumerator.GetValue());
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		EndLeafRow();
	}

	void DrawSequence(const char* label, const PgE::SequenceFacet& sequence, const PgE::TypedRef& ref)
	{
		const std::size_t size = sequence.Size(ref);
		const bool open = BeginTreeRow(label);
		ImGui::TextDisabled("(%zu)", size);
		if (!open)
		{
			return;
		}

		for (std::size_t index = 0; index < size; ++index)
		{
			ImGui::PushID(static_cast<int>(index));

			const std::string elementLabel = std::format("[{}]", index);
			DrawValue(elementLabel.c_str(), sequence.ElementRef(ref, index));

			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	void DrawContents(const PgE::TypedRef& ref)
	{
		if (ref.Type->GetKind() == PgE::TypeKind::Class && ref.Type->GetFacets().empty())
		{
			DrawFields(ref);
			return;
		}

		// Reached through drawing a non-record/faceted type directly, or when dereferencing a non-record/faceted type
		DrawValue("value", ref);
	}

	void DrawPointer(const char* label, const PgE::TypedRef& ref)
	{
		const auto pointee = ref.Dereference();
		if (!pointee)
		{
			DrawDisabledText(label, pointee.error().Reason == PgE::DereferenceError::NullPointer ? "<null>" : "<unsupported>");
			return;
		}

		const bool open = BeginTreeRow(label);
		ImGui::TextDisabled("%p", pointee->Data);
		if (open)
		{
			DrawContents(*pointee);
			ImGui::TreePop();
		}
	}

	void DrawValue(const char* label, const PgE::TypedRef& ref)
	{
		if (DrawablePrimitives::TryDraw(label, ref))
		{
			return;
		}

		// Facets before the structural walk: a superseding facet empties the field view, so recursing first
		// would draw an empty node for a string or a container.
		if (const PgE::EnumerationFacet* enumeration = ref.Type->GetFacet<PgE::EnumerationFacet>())
		{
			DrawEnumeration(label, *enumeration, ref);
			return;
		}

		if (const PgE::StringFacet* string = ref.Type->GetFacet<PgE::StringFacet>())
		{
			const std::string_view view = string->View(ref);
			DrawDisabledText(label, std::format("\"{}\"", view).c_str());
			return;
		}

		if (const PgE::SequenceFacet* sequence = ref.Type->GetFacet<PgE::SequenceFacet>())
		{
			DrawSequence(label, *sequence, ref);
			return;
		}

		if (ref.Type->GetKind() == PgE::TypeKind::Pointer)
		{
			DrawPointer(label, ref);
			return;
		}

		if (ref.Type->GetKind() == PgE::TypeKind::Union)
		{
			DrawDisabledText(label, "<union>");
			return;
		}

		if (ref.Type->GetKind() == PgE::TypeKind::Class)
		{
			if (BeginTreeRow(label))
			{
				DrawFields(ref);
				ImGui::TreePop();
			}
			return;
		}

		DrawDisabledText(label, ref.Type->CanStringify() ? ref.Type->Stringify(ref).c_str() : "<unsupported>");
	}
}

namespace PgE
{
	void DebugPanelDrawer::DrawObject(const TypedRef& object)
	{
		// One table for the whole panel, opened once here: every row below writes into the same two columns,
		// so a nested subobject's values stay aligned with the top level instead of measuring its own.
		if (!ImGui::BeginTable("DebugPanelFields", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			return;
		}

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, NameColumnWeight);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, ValueColumnWeight);

		DrawContents(object);

		ImGui::EndTable();
	}
}
