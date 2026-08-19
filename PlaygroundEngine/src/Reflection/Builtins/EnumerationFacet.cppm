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
								 const std::span<const std::string_view> scopePath,
								 const std::uint64_t value,
								 const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo(identifier, displayName, scopePath, annotations), _value(value)
		{}

		std::uint64_t GetValue() const
		{
			return _value;
		}

	private:
		std::uint64_t _value;
	};

	// The one statement of the enumerator table's currency, in both directions. Every path that crosses
	// between an enum and a uint64 goes through these, so the compile-time table, the erased thunks, and the
	// typed sugar cannot spell the conversion differently: a negative enumerator sign-extends once, here.

	export template <typename Enum>
	requires std::is_enum_v<Enum>
	constexpr std::uint64_t ToEnumeratorValue(const Enum value)
	{
		return static_cast<std::uint64_t>(static_cast<std::underlying_type_t<Enum>>(value));
	}

	export template <typename Enum>
	requires std::is_enum_v<Enum>
	constexpr Enum FromEnumeratorValue(const std::uint64_t value)
	{
		return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(value));
	}

	export class EnumerationFacet
	{
		// The enum-specific facet of a TypeInfo: the enumerator set plus the underlying integer type.

	public:
		// Supersedes the raw structural view like the other in-table facets; read generically by the builder.
		static constexpr bool Supersedes = true;

		// An enumeration is always readable and always writable, so neither thunk is nullable and there are
		// no capability queries, unlike the string and sequence facets whose views can be read-only.
		using ValueThunk = std::uint64_t (*)(const void*);
		using AssignThunk = void (*)(void*, std::uint64_t);

		constexpr EnumerationFacet(const TypeReference underlyingType,
								   const std::span<const EnumeratorInfo> enumerators,
								   const ValueThunk value,
								   const AssignThunk assign)
			: _underlyingType(underlyingType), _enumerators(enumerators), _value(value), _assign(assign)
		{}

		const TypeInfo& GetUnderlyingType() const;

		// The object's value in the enumerator table's currency: the underlying integer widened to uint64,
		// which is what FindByValue and EnumeratorInfo::GetValue speak. A consumer holding only an erased
		// pointer cannot do this conversion itself, since the width and signedness live in the enum type.
		std::uint64_t Value(const TypedRef& object) const pre(_value != nullptr) pre(CheckFacetOwner(_owner, object).has_value())
		{
			return _value(object.Data);
		}

		std::expected<void, FacetError> Assign(const TypedRef& object, const std::uint64_t value) const pre(_assign != nullptr)
		{
			if (const auto owned = CheckFacetOwner(_owner, object); !owned)
			{
				return owned;
			}
			if (object.IsConst)
			{
				return std::unexpected(FacetError{FacetError::ConstViolation});
			}

			_assign(object.Data, value);
			return {};
		}

		std::span<const EnumeratorInfo> GetEnumerators() const
		{
			return _enumerators;
		}

		const EnumeratorInfo* FindByIdentifier(std::string_view identifier) const;
		const EnumeratorInfo* FindByValue(std::uint64_t value) const;

		// Set by the facet builder to the type that provides this facet, so an op can tell an object of that
		// type from any other. Empty for a facet built by hand, which skips the check.
		constexpr void SetOwnerType(const TypeReference owner)
		{
			_owner = owner;
		}

	private:
		TypeReference _owner;
		TypeReference _underlyingType;
		std::span<const EnumeratorInfo> _enumerators;
		ValueThunk _value = nullptr;
		AssignThunk _assign = nullptr;
	};
}
