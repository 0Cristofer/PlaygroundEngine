module PlaygroundEngine.Reflection.Core;

import :TypeInfo;
import :FieldInfo;
import :TypedRef;

import std;

namespace PgE
{
	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj)
	{
		// A type with a stringify thunk renders through it: a formattable leaf, the type-name placeholder, or a
		// facet that installed its own rendering (a quoted string, a "[...]" sequence, an enumerator name). The
		// renderer names no facet; each facet's rendering lives in the thunk its TypeInfoTraits set, and a
		// sequence thunk recurses back into ObjectToString for its elements. A type with no thunk is a struct,
		// rendered by walking its fields.
		if (typeInfo.CanStringify())
		{
			return typeInfo.Stringify(obj);
		}

		std::string out = "{";
		bool firstField = true;
		for (const FieldInfo& field : typeInfo.GetFields())
		{
			if (!firstField)
			{
				out += ", ";
			}
			firstField = false;
			out += field.GetIdentifier();
			out += ": ";

			// Prefer the borrow: it reads the field in place, whatever its size. The stack-slot path is the
			// fallback for a non-addressable field (a bitfield), which is always a small trivial type that
			// fits the slot; running it for an addressable struct or container would overflow the slot.
			if (const auto ref = field.GetRef(obj))
			{
				out += ObjectToString(field.GetTypeInfo(), ref->Data);
			}
			else
			{
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
		}
		return out + "}";
	}
}
