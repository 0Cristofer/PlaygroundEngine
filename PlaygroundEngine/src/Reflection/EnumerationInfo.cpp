module PlaygroundEngine.Reflection;

import :EnumerationInfo;

import std;

namespace PgE
{
	const TypeInfo& EnumerationInfo::GetUnderlyingType() const
	{
		return *_underlyingType;
	}

	const EnumeratorInfo* EnumerationInfo::FindByIdentifier(const std::string_view identifier) const
	{
		// Linear scan: enumerator counts are small and lookups happen at boundaries (serialize, edit),
		// never in the frame loop. Same rationale as TypeInfo's field/function lookups.
		for (const EnumeratorInfo& enumerator : _enumerators)
		{
			if (enumerator.GetIdentifier() == identifier)
				return &enumerator;
		}

		return nullptr;
	}

	const EnumeratorInfo* EnumerationInfo::FindByValue(const std::uint64_t value) const
	{
		// The first enumerator matching the bit pattern wins; aliased enumerators (two names, one value)
		// resolve to the first declared, which is the deterministic choice serialization needs.
		for (const EnumeratorInfo& enumerator : _enumerators)
		{
			if (enumerator.GetValue() == value)
				return &enumerator;
		}

		return nullptr;
	}
}
