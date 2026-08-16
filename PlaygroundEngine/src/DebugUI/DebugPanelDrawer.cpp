module PlaygroundEngine.DebugUi;

import :DebugPanelDrawer;

import imgui;

import PlaygroundEngine.Reflection;

import std;

namespace
{
	constexpr float DragSpeed = 0.01f;

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

	template <typename Primitive>
	bool TryDrawPrimitive(const char* label, const PgE::TypedRef ref)
	{
		if (ref.Type != &PgE::TypeMetaOf<Primitive>())
		{
			return false;
		}

		ImGui::BeginDisabled(ref.IsConst);
		if constexpr (std::is_same_v<Primitive, bool>)
		{
			ImGui::Checkbox(label, static_cast<bool*>(ref.Data));
		}
		else
		{
			ImGui::DragScalar(label, ImGuiDataTypeOf<Primitive>(), ref.Data, DragSpeed);
		}
		ImGui::EndDisabled();

		return true;
	}

	template <typename... Types>
	struct PrimitiveTypes
	{
		// The fold short-circuits, so the first type whose identity matches owns the row and the rest are
		// never called. A false result is the "not a primitive" answer the caller dispatches on.
		static bool TryDraw(const char* label, const PgE::TypedRef ref)
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

	void DrawValue(const char* label, PgE::TypedRef value);

	PgE::TypedRef PeelQualifiers(PgE::TypedRef ref)
	{
		// A cv node carries no structure of its own, so a borrow is only drawable once peeled down to the
		// unqualified type. const Foo* reaches Foo through a const node, and that node is what says the
		// target must not be edited.
		while (ref.Type->GetTraits().IsConst || ref.Type->GetTraits().IsVolatile)
		{
			ref.IsConst = ref.IsConst || ref.Type->GetTraits().IsConst;
			ref.Type = &ref.Type->GetInnerType();
		}

		return ref;
	}

	void DrawDisabledText(const char* label, const char* text)
	{
		ImGui::BeginDisabled();
		ImGui::LabelText(label, "%s", text);
		ImGui::EndDisabled();
	}

	void DrawFields(const PgE::TypedRef object)
	{
		bool drewAnnotatedField = false;

		for (const PgE::FieldInfo& field : object.Type->GetFields())
		{
			if (!field.HasAnnotation<PgE::DrawDebug>())
			{
				continue;
			}

			drewAnnotatedField = true;
			const char* label = field.GetIdentifier().data();

			const auto ref = field.GetRef(object.Data);
			if (!ref)
			{
				// A bitfield has no address, so there is no borrow to edit in place.
				DrawDisabledText(label, "<not addressable>");
				continue;
			}

			DrawValue(label, PgE::TypedRef{.Type = ref->Type, .Data = ref->Data, .IsConst = object.IsConst || ref->IsConst});
		}

		if (!drewAnnotatedField)
		{
			ImGui::TextDisabled("<no annotated fields>");
		}
	}

	void DrawEnumeration(const char* label, const PgE::EnumerationFacet& enumeration, const PgE::TypedRef ref)
	{
		const std::uint64_t current = enumeration.Value(ref.Data);
		const PgE::EnumeratorInfo* selected = enumeration.FindByValue(current);

		ImGui::BeginDisabled(ref.IsConst);
		if (ImGui::BeginCombo(label, selected != nullptr ? selected->GetIdentifier().data() : "<unnamed>"))
		{
			for (const PgE::EnumeratorInfo& enumerator : enumeration.GetEnumerators())
			{
				if (const bool isSelected = enumerator.GetValue() == current; ImGui::Selectable(enumerator.GetIdentifier().data(), isSelected))
				{
					enumeration.Assign(ref.Data, enumerator.GetValue());
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	void DrawSequence(const char* label, const PgE::SequenceFacet& sequence, const PgE::TypedRef ref)
	{
		const std::size_t size = sequence.Size(ref.Data);
		if (!ImGui::TreeNode(label, "%s (%zu)", label, size))
		{
			return;
		}

		const bool elementsReadOnly = ref.IsConst || !sequence.CanMutateElements();

		for (std::size_t index = 0; index < size; ++index)
		{
			ImGui::PushID(static_cast<int>(index));

			PgE::TypedRef element =
				elementsReadOnly ? sequence.ElementRef(static_cast<const void*>(ref.Data), index) : sequence.ElementRef(ref.Data, index);
			element.IsConst = element.IsConst || elementsReadOnly;

			const std::string elementLabel = std::format("[{}]", index);
			DrawValue(elementLabel.c_str(), element);

			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	void DrawContents(const PgE::TypedRef object)
	{
		const PgE::TypedRef ref = PeelQualifiers(object);

		// A facet is the type's own view of itself and supersedes the structural one, so a faceted type
		// reports no fields and has to draw through the labeled path. Only a plain record draws inline.
		const bool isRecord = ref.Type->GetKind() == PgE::TypeKind::Class || ref.Type->GetKind() == PgE::TypeKind::Union;
		if (isRecord && ref.Type->GetFacets().empty())
		{
			DrawFields(ref);
			return;
		}

		DrawValue("value", ref);
	}

	void DrawPointer(const char* label, const PgE::TypedRef ref)
	{
		void* pointee = *static_cast<void**>(ref.Data);
		if (pointee == nullptr)
		{
			DrawDisabledText(label, "<null>");
			return;
		}

		if (ImGui::TreeNode(label, "%s (%p)", label, pointee))
		{
			DrawContents(PgE::TypedRef{.Type = &ref.Type->GetInnerType(), .Data = pointee, .IsConst = ref.IsConst});
			ImGui::TreePop();
		}
	}

	void DrawValue(const char* label, const PgE::TypedRef value)
	{
		const PgE::TypedRef ref = PeelQualifiers(value);

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
			const std::string_view view = string->View(ref.Data);
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

		if (ref.Type->GetKind() == PgE::TypeKind::Class || ref.Type->GetKind() == PgE::TypeKind::Union)
		{
			if (ImGui::TreeNode(label))
			{
				DrawFields(ref);
				ImGui::TreePop();
			}
			return;
		}

		DrawDisabledText(label, ref.Type->CanStringify() ? ref.Type->Stringify(ref.Data).c_str() : "<unsupported>");
	}
}

namespace PgE
{
	void DebugPanelDrawer::DrawObject(const TypedRef object)
	{
		DrawContents(object);
	}
}
