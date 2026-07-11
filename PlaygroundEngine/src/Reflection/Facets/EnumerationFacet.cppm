export module PlaygroundEngine.Reflection:EnumerationFacet;

import :DeclarationInfo;
import :TypeReference;

import std;

namespace PgE
{
	export class TypeInfo;

	export class EnumeratorInfo : public DeclarationInfo
	{
		// One named constant of an enumeration. The value is the enumerator's underlying integer stored as
		// its raw bit pattern (a signed negative wraps to two's complement); the owning EnumerationFacet's
		// underlying TypeInfo is what says how to read those bits back.

	public:
		constexpr EnumeratorInfo(const std::string_view identifier, const std::string_view displayName,
		                         const std::uint64_t value,
		                         const std::span<const AnnotationInfo> annotations) :
			DeclarationInfo(identifier, displayName, annotations), _value(value)
		{
		}

		std::uint64_t GetValue() const { return _value; }

	private:
		std::uint64_t _value;
	};

	export class EnumerationFacet
	{
		// The enum-specific facet of a TypeInfo: the enumerator set plus the underlying integer type. It is
		// stored in the facet table like StringFacet and SequenceFacet, present only on TypeInfos whose kind
		// is Enum, so a null GetEnumeration() is the self-validating "not an enumeration" answer. Whether the
		// enumeration is a bit-flag set is not stored here; that is an annotation on the type, read through
		// HasAnnotation.

	public:
		// Supersedes the raw structural view like the other in-table facets; read generically by the builder.
		static constexpr bool Supersedes = true;

		constexpr EnumerationFacet(const TypeReference underlyingType,
		                           const std::span<const EnumeratorInfo> enumerators,
		                           std::uint64_t (*readValue)(const void*)) :
			_underlyingType(underlyingType), _enumerators(enumerators), _readValue(readValue)
		{
		}

		const TypeInfo& GetUnderlyingType() const;

		std::span<const EnumeratorInfo> GetEnumerators() const { return _enumerators; }

		const EnumeratorInfo* FindByIdentifier(std::string_view identifier) const;
		const EnumeratorInfo* FindByValue(std::uint64_t value) const;

		// Reads the enum's underlying integer from erased memory as the raw uint64 bit pattern the
		// enumerators are stored in (the one typed step the erased renderer cannot do itself), so
		// FindByValue can match it. A signed negative wraps to two's complement, as it does on capture.
		std::uint64_t ReadValue(const void* obj) const { return _readValue(obj); }

	private:
		TypeReference _underlyingType;
		std::span<const EnumeratorInfo> _enumerators;
		std::uint64_t (*_readValue)(const void*);
	};
}
