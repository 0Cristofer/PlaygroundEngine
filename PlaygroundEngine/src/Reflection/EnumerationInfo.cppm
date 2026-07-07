export module PlaygroundEngine.Reflection:EnumerationInfo;

import :DeclarationInfo;

import std;

namespace PgE
{
	export class TypeInfo;

	export class EnumeratorInfo : public DeclarationInfo
	{
		// One named constant of an enumeration. The value is the enumerator's underlying integer stored as
		// its raw bit pattern (a signed negative wraps to two's complement); the owning EnumerationInfo's
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

	export class EnumerationInfo
	{
		// The enum-specific facet of a TypeInfo: the enumerator set plus the underlying integer type. It
		// exists only on TypeInfos whose kind is Enum, so a null GetEnumeration() is the self-validating
		// "not an enumeration" answer. Whether the enumeration is a bit-flag set is not stored here; that
		// is an annotation on the type, read through HasAnnotation.

	public:
		constexpr EnumerationInfo(const TypeInfo* underlyingType,
		                          const std::span<const EnumeratorInfo> enumerators) :
			_underlyingType(underlyingType), _enumerators(enumerators)
		{
		}

		const TypeInfo& GetUnderlyingType() const;

		std::span<const EnumeratorInfo> GetEnumerators() const { return _enumerators; }

		const EnumeratorInfo* FindByIdentifier(std::string_view identifier) const;
		const EnumeratorInfo* FindByValue(std::uint64_t value) const;

	private:
		const TypeInfo* _underlyingType = nullptr;
		std::span<const EnumeratorInfo> _enumerators;
	};
}
