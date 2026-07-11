module PlaygroundEngine.Reflection;

namespace PgE
{
	std::string ObjectToString(const TypeInfo& typeInfo, const void* obj)
	{
		// Facet-first: a superseding protocol renders the object rather than its structure. Strings and
		// sequences render structurally; an enumeration renders as its enumerator name (or the underlying
		// number when no enumerator matches) through its EnumerationFacet, not the leaf stringify thunk.
		if (const StringFacet* string = typeInfo.GetFacet<StringFacet>())
		{
			return std::format("\"{}\"", string->View(obj));
		}

		if (const SequenceFacet* sequence = typeInfo.GetFacet<SequenceFacet>())
		{
			constexpr std::size_t longSequenceThreshold = 10;
			const std::size_t count = sequence->Size(obj);
			const bool isLongSequence = count > longSequenceThreshold;
			const std::size_t elementsToRender = isLongSequence ? longSequenceThreshold : count;

			std::string out = "[";
			for (std::size_t index = 0; index < elementsToRender; ++index)
			{
				if (index != 0)
				{
					out += ", ";
				}
				const TypedRef element = sequence->ElementRef(obj, index);
				out += ObjectToString(*element.Type, element.Data);
			}
			if (isLongSequence)
			{
				out += ", ...";
			}
			return out + "]";
		}

		if (const EnumerationFacet* enumeration = typeInfo.GetFacet<EnumerationFacet>())
		{
			if (const EnumeratorInfo* enumerator = enumeration->FindByValue(enumeration->ReadValue(obj)))
			{
				return std::string(enumerator->GetIdentifier());
			}

			// No enumerator names this value (a flag combination or an in-range static_cast): render the
			// underlying integer, which shares the enum's bytes, through its own stringify thunk.
			return enumeration->GetUnderlyingType().Stringify(obj);
		}

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
