export module PlaygroundEngine.Reflection.Builtins:EnumerationFacet;

import PlaygroundEngine.Reflection.Core;

import std;

namespace PgE
{
	export class EnumeratorInfo : public DeclarationInfo
	{
		// One named constant of an enumeration. The value is the enumerator's underlying integer stored as
		// its raw bit pattern (a signed negative wraps to two's complement); the owning EnumerationFacet's
		// underlying TypeInfo is what says how to read those bits back.

	public:
		constexpr EnumeratorInfo(const std::string_view identifier,
								 const std::string_view displayName,
								 const std::uint64_t value,
								 const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, annotations), _value(value)
		{}

		std::uint64_t GetValue() const { return _value; }

	private:
		std::uint64_t _value;
	};

	export class EnumerationFacet
	{
		// The enum-specific facet of a TypeInfo: the enumerator set plus the underlying integer type.

	public:
		// Supersedes the raw structural view like the other in-table facets; read generically by the builder.
		static constexpr bool Supersedes = true;

		constexpr EnumerationFacet(const TypeReference underlyingType, const std::span<const EnumeratorInfo> enumerators)
			: _underlyingType(underlyingType), _enumerators(enumerators)
		{}

		const TypeInfo& GetUnderlyingType() const;

		std::span<const EnumeratorInfo> GetEnumerators() const { return _enumerators; }

		const EnumeratorInfo* FindByIdentifier(std::string_view identifier) const;
		const EnumeratorInfo* FindByValue(std::uint64_t value) const;

	private:
		TypeReference _underlyingType;
		std::span<const EnumeratorInfo> _enumerators;
	};
}
