module;

#include <meta>

export module PlaygroundTests.ReflectionTestTypes;

import std;
import PlaygroundEngine.Reflection;

// Shared fixtures for the reflection test suite. Each Reflection*Tests.cpp imports this so the reflected
// types (and their deterministic display names) are defined once. A named namespace (not anonymous) keeps
// reflected member display strings stable across runs.
export namespace ReflectionTestTypes
{
	// ReSharper disable CppDeclaratorNeverUsed
	// ReSharper disable CppParameterMayBeConst
	// ReSharper disable CppPassValueParameterByConstReference
	// ReSharper disable CppEnumeratorNeverUsed
	// ReSharper disable CppMemberFunctionMayBeStatic
	// ReSharper disable CppMemberFunctionMayBeConst
	// Mirrors the shape the game used to probe at startup: a string leaf and an int.
	struct Person
	{
		std::string Name;
		int Age;
	};

	// Fundamental-typed members only, so reflected signatures have stable display
	// strings (std::string's display name is implementation-defined).
	struct Widget
	{
		int Width;
		int Height;

		int Area() const
		{
			return Width * Height;
		}

		void Resize(int w, int h)
		{
			Width = w;
			Height = h;
		}
	};

	struct Counter
	{
		int Value = 0;

		int Get() const
		{
			return Value;
		}

		void Add(int amount)
		{
			Value += amount;
		}
	};

	struct Accessor
	{
		int Value = 5;

		int& Mutable()
		{
			return Value;
		}

		const int& Readonly() const
		{
			return Value;
		}
	};

	struct MoveOnly
	{
		int Tag = 0;

		MoveOnly() = default;
		MoveOnly(const MoveOnly&) = delete;
		MoveOnly(MoveOnly&&) = default;
	};

	struct Sink
	{
		int Value = 0;

		void Consume(MoveOnly item)
		{
			Value = item.Tag;
		}
	};

	// Bitfields (no addressable byte) plus a plain field, to prove field access does not depend on
	// taking a member's address.
	struct Packet
	{
		unsigned Version : 4 = 3;
		unsigned Flags : 4 = 10;
		int Payload = 100;
	};

	// Private members, to prove reflected access ignores access control (the point of the feature).
	class Secret
	{
	public:
		int Reveal() const
		{
			return _hidden;
		}

	private:
		int _hidden = 7;
	};

	// A const member has no setter (NotWritable) but is still readable.
	struct Fixed
	{
		const int Constant = 5;
		int Variable = 1;
	};

	// Reference members: tagged by the referent type, read through the reference, never rebindable.
	struct Referencing
	{
		int Target = 100;
		int& Alias;
		const int& ConstAlias;

		Referencing() : Alias(Target), ConstAlias(Target)
		{}
	};

	// Move-only but move-assignable (like unique_ptr / Poly): settable through a move, not a copy.
	struct MoveOnlyValue
	{
		int Tag = 0;
		MoveOnlyValue() = default;
		MoveOnlyValue(const MoveOnlyValue&) = delete;
		MoveOnlyValue(MoveOnlyValue&&) = default;
		MoveOnlyValue& operator=(MoveOnlyValue&&) = default;
	};

	struct MoveOnlyHolder
	{
		MoveOnlyValue Item;
	};

	// Neither copyable nor movable (like std::mutex): reachable only through the borrow.
	struct Immovable
	{
		int Tag = 0;
		Immovable() = default;
		Immovable(const Immovable&) = delete;
		Immovable(Immovable&&) = delete;
		Immovable& operator=(const Immovable&) = delete;
		Immovable& operator=(Immovable&&) = delete;
	};

	struct ImmovableHolder
	{
		Immovable Item;
	};

	// A nested reflected struct plus a pointer member.
	struct Inner
	{
		int A = 1;
	};

	struct Outer
	{
		Inner Child;
		int* Pointer = nullptr;
		int Plain = 7;
	};

	// Tracks whether an instance was moved-from, to observe "invoke"'s opt-in move.
	struct Tracked
	{
		int Value = 0;
		bool Moved = false;
		Tracked() = default;

		Tracked(const Tracked& other) : Value(other.Value)
		{}

		Tracked(Tracked&& other) noexcept : Value(other.Value)
		{
			other.Moved = true;
		}

		Tracked& operator=(const Tracked&) = default;
		Tracked& operator=(Tracked&&) = default;
	};

	struct Consumer
	{
		int Stored = 0;

		void Store(Tracked item)
		{
			Stored = item.Value;
		}
	};

	// Copyable but with a deleted move constructor: the invoke binder must still copy it by value.
	struct CopyOnlyParam
	{
		int Value = 0;
		CopyOnlyParam() = default;
		CopyOnlyParam(const CopyOnlyParam&) = default;
		CopyOnlyParam(CopyOnlyParam&&) = delete;
		CopyOnlyParam& operator=(const CopyOnlyParam&) = default;
	};

	struct CopyConsumer
	{
		int Stored = 0;

		void Take(CopyOnlyParam item)
		{
			Stored = item.Value;
		}
	};

	// A type whose own metadata names itself: a factory returning it by value, a method taking it by
	// reference, and one returning it by reference. Each self-reference needs the type's TypeInfo before
	// its construction has finished; the lazy TypeReference is what lets construction close first.
	struct Node
	{
		int Value = 0;

		Node Clone() const
		{
			return Node{Value};
		}
		void CopyFrom(const Node& other)
		{
			Value = other.Value;
		}
		Node& Self()
		{
			return *this;
		}
	};

	// Two types that name each other by value: building either needs the other's TypeInfo, which needs
	// the first, a mutual cycle the lazy resolver breaks. Declared here, defined once both are complete.
	struct Pong;

	struct Ping
	{
		Pong Bounce() const;
	};

	struct Pong
	{
		Ping Bounce() const;
	};

	inline Pong Ping::Bounce() const
	{
		return Pong{};
	}
	inline Ping Pong::Bounce() const
	{
		return Ping{};
	}

	// Private member functions are reflected and invocable, the way private fields already are.
	struct WithPrivate
	{
		int Value = 0;

	private:
		int Doubled() const
		{
			return Value * 2;
		}
		void SetValue(int v)
		{
			Value = v;
		}
	};

	// A rvalue-ref-qualified overload cannot be called on the erased lvalue object the thunk holds, so it
	// reflects as metadata with no invoker (like a bitfield reflecting with no borrow). Normal overloads on
	// the same type stay invocable.
	struct RefQualified
	{
		int Value = 0;

		int OnRvalue() &&
		{
			return Value;
		}
		int OnAny() const
		{
			return Value;
		}
	};

	// An enum with the default (int) underlying type. Stringifies to its enumerator name, which also
	// propagates through Palette's Primary field.
	enum class Shade
	{
		Red,
		Green
	};

	struct Palette
	{
		Shade Primary = Shade::Green;
		int Count = 2;
	};

	// A leaf with no std::format support, no trait, and no enumerators: reflectable, stringified by the
	// type-name fallback.
	struct Opaque
	{};

	// Enum with an explicit unsigned underlying type and gappy, flag-shaped values. None = 0 exercises the
	// zero-valued lookup; a bit-or'd combination (Read | Write) is a valid value no enumerator names.
	enum class Permissions : std::uint16_t
	{
		None = 0,
		Read = 1,
		Write = 2,
		Execute = 4
	};

	// Signed underlying type with a negative enumerator, to pin the two's-complement round-trip through the
	// uint64 bit pattern.
	enum class Temperature : std::int16_t
	{
		Freezing = -10,
		Boiling = 100
	};

	// Unscoped enum, to confirm the enumerator walk is not scoped-only.
	enum Direction
	{
		North,
		South,
		East,
		West
	};

	struct Base
	{
		virtual ~Base() = default;
		int V = 1;

		virtual int GetV()
		{
			return V;
		}
	};

	struct Child : Base
	{
		int GetV() override
		{
			return V + 1;
		}
	};

	struct ChildNoOverride : Base
	{};

	// Polymorphic and abstract: cannot be constructed, so its traits differ from a concrete class.
	struct AbstractShape
	{
		virtual ~AbstractShape() = default;
		virtual int Sides() const = 0;
	};

	union Scalar
	{
		int AsInt;
		float AsFloat;
	};

	// Value-carrying annotations plus an empty tag. Doc stores its string through
	// define_static_string so the value is a self-contained constant (see the reflection
	// design notes on structural annotation values).
	struct Range
	{
		double Min;
		double Max;
	};

	struct Doc
	{
		const char* Text;

		consteval Doc(const char* text) : Text(std::define_static_string(std::string_view{text}))
		{}
	};

	struct Serializable
	{};

	// One type carrying annotations on every declaration kind: the type itself, a field,
	// a member function, and a parameter. Exercises the shared Annotated surface uniformly.
	struct[[= Serializable{}]] Gadget
	{
		[[= Range{0.0, 100.0}]][[= Doc{"hit points"}]] int Health = 100;

		// The same annotation type repeated: legal, kept in declaration order, not deduplicated.
		[[= Doc{"primary"}]][[= Doc{"secondary"}]] int Nickname = 0;

		int Plain = 0;

		[[= Doc{"apply damage, return remaining health"}]] int Hurt([[= Range{0.0, 1000.0}]] int amount)
		{
			Health -= amount;
			return Health;
		}
	};

	// A struct whose fields are reflected containers: the field walk recurses into each field's facet
	// rather than the container's structure.
	struct Inventory
	{
		std::vector<int> Slots;
		std::string Owner;
	};

	// A facet defined entirely outside the reflection library. It omits Supersedes, so it adds information
	// alongside the structural view rather than replacing it.
	struct LabelFacet
	{
		std::string_view Text;
	};

	// A plain type that will be given the user facet through a TypeInfoTraits specialization, with no change
	// to the reflection core.
	struct Labeled
	{
		int Value = 1;
	};

	// ReSharper restore CppMemberFunctionMayBeConst
	// ReSharper restore CppMemberFunctionMayBeStatic
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
	// ReSharper restore CppPassValueParameterByConstReference
	// ReSharper restore CppEnumeratorNeverUsed
}

// A user extends the facet system from outside the library: specialize TypeInfoTraits and return the
// facet from MakeFacets; the core assembles and keys it with no change. Non-exported but reachable to
// importers through this module (GCC 16 cross-module reachability), which TypeOf<Labeled> picks up.
template <>
struct PgE::TypeInfoTraits<ReflectionTestTypes::Labeled> : PgE::TypeInfoTraitsDefaults
{
	static consteval auto MakeFacets()
	{
		return std::tuple{ReflectionTestTypes::LabelFacet{.Text = "the-label"}};
	}
};
