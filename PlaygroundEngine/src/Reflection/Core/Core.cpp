module PlaygroundEngine.Reflection.Core;

import :BaseInfo;
import :TypeInfo;
import :FieldInfo;
import :TypedRef;

import std;

namespace PgE
{
	namespace
	{
		bool HasMembersToWalk(const TypeInfo& typeInfo)
		{
			return !typeInfo.GetFields().empty() || !typeInfo.GetBases().empty();
		}

		bool RendersThroughFacet(const TypeInfo& typeInfo)
		{
			// Facet-agnostic on purpose: a facet that supersedes a type suppresses its fields and bases, so
			// "holds state but nothing to walk" is exactly the shape of a facet-backed type (a std::string).
			return !HasMembersToWalk(typeInfo) && !typeInfo.GetFacets().empty() && typeInfo.CanStringify();
		}

		void BeginEntry(std::string& out, bool& firstEntry, const std::string_view identifier)
		{
			if (!firstEntry)
			{
				out += ", ";
			}
			firstEntry = false;
			out += identifier;
			out += ": ";
		}

		void AppendFieldValue(std::string& out, const FieldInfo& field, const void* obj)
		{
			// Prefer the borrow: it reads the field in place, whatever its size. The stack-slot path is the
			// fallback for a non-addressable field (a bitfield), which is always a small trivial type that
			// fits the slot; running it for an addressable struct or container would overflow the slot.
			if (const auto ref = field.GetRef(obj))
			{
				out += ObjectToString(field.GetTypeInfo(), ref->Data);
				return;
			}

			alignas(std::uintmax_t) std::byte slot[sizeof(std::uintmax_t)];
			if (field.GetValue(obj, TypedRef{.Type = &field.GetTypeInfo(), .Data = slot, .IsConst = false}))
			{
				out += ObjectToString(field.GetTypeInfo(), slot);
			}
			else
			{
				out += "<unreadable>";
			}
		}

		void AppendMembers(std::string& out, bool& firstEntry, const TypeInfo& typeInfo, const void* obj)
		{
			// GetFields() is direct members only, so the inherited ones are reached by walking the bases, each
			// through the base subobject its field thunks were built against.
			for (const BaseInfo& base : typeInfo.GetBases())
			{
				const TypeInfo& baseType = base.GetTypeInfo();
				const void* baseObject = base.Upcast(obj);

				if (HasMembersToWalk(baseType))
				{
					AppendMembers(out, firstEntry, baseType, baseObject);
				}
				else if (RendersThroughFacet(baseType))
				{
					// The display name, not the identifier: a specialization (a std::string base) has no
					// identifier of its own and would label the entry with an empty name.
					BeginEntry(out, firstEntry, baseType.GetDisplayName());
					out += baseType.Stringify(baseObject);
				}

				// Anything else is an empty base (a tag or policy type): it holds no state, so it contributes
				// no entry rather than the type-name placeholder its leaf thunk would produce.
			}

			for (const FieldInfo& field : typeInfo.GetFields())
			{
				BeginEntry(out, firstEntry, field.GetIdentifier());
				AppendFieldValue(out, field, obj);
			}
		}
	}

	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj)
	{
		// Anything with members renders by walking them; the thunk is for a leaf, which is what a type with
		// nothing to walk is (an int, an empty struct, a facet-backed string). The builder hands out a thunk
		// by fields alone, so which one wins is the renderer's call. See docs/ReflectionInternals.md (Rendering).
		if (!HasMembersToWalk(typeInfo) && typeInfo.CanStringify())
		{
			return typeInfo.Stringify(obj);
		}

		std::string out = "{";
		bool firstEntry = true;
		AppendMembers(out, firstEntry, typeInfo, obj);
		return out + "}";
	}
}
