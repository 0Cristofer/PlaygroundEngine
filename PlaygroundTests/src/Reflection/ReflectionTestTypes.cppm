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
	// Namespace-scope entities, reflected by being named rather than discovered. FreeMaxSlots is the
	// constant-readable case whose address must never be taken (it has no out-of-line definition).
	int FreeSpawn(const int count, const float scale)
	{
		return static_cast<int>(count * scale);
	}

	int FreeCounter = 7;
	constexpr int FreeMaxSlots = 8;

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

	// An rvalue-reference data member: shares the referent's decayed tag like the lvalue aliases, told apart
	// only by IsRvalueReference. Bound in the constructor, never rebindable.
	struct RvalueField
	{
		int Owned = 1;
		int&& Moved;

		RvalueField() : Moved(std::move(Owned))
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

	// Takes a pointer by value, so the argument object is the pointer itself: a null pointer is a value the
	// binder must pass through, not an argument that names no object.
	struct PointerSink
	{
		int Owned = 42;
		int* Received = nullptr;
		bool Called = false;

		void Aim(int* target)
		{
			Received = target;
			Called = true;
		}

		// Distinct names, not overloads: all three parameter forms erase to the same int* tag, so an
		// overload set here would rank binder shapes rather than exercise them one at a time.
		void Retarget(int*& target)
		{
			target = &Owned;
			Called = true;
		}

		void Peek(int* const& target)
		{
			Received = target;
			Called = true;
		}
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

	// Deducing-this members: the explicit object parameter is dropped from the reflected parameters, and the
	// const-ness / ref-qualification are read off it, not the function. All stay invocable through the object.
	struct Deducing
	{
		int Base = 10;

		int Get(this const Deducing& self, int add)
		{
			return self.Base + add;
		}
		int Bump(this Deducing& self, int add)
		{
			return self.Base += add;
		}

		// By-value object parameter: copies the object, so it is const-callable like a const-ref one.
		int Copy(this Deducing self, int add)
		{
			return self.Base + add;
		}

	private:
		int Secret(this const Deducing& self, int factor)
		{
			return self.Base * factor;
		}
	};

	// Overloaded operators and user-defined conversions: reflected into their own lists (GetOperators /
	// GetConversions), each still invocable through the shared function machinery. Copy/move assignment are
	// special members, so they stay out of the operator list; the converting assignment operator=(int) stays.
	struct Coord
	{
		int Value = 0;

		Coord operator+(const Coord& rhs) const
		{
			return Coord{Value + rhs.Value};
		}
		bool operator==(const Coord& rhs) const
		{
			return Value == rhs.Value;
		}
		int operator[](int index) const
		{
			return Value + index;
		}
		Coord& operator=(int value)
		{
			Value = value;
			return *this;
		}

		explicit operator int() const
		{
			return Value;
		}
		operator bool() const
		{
			return Value != 0;
		}

		// A hidden friend: found only by argument-dependent lookup, and not a class member despite being
		// declared in the class. It takes both operands as arguments, so it is called with no object.
		friend Coord operator*(const Coord& lhs, const Coord& rhs)
		{
			return Coord{lhs.Value * rhs.Value};
		}

	private:
		Coord operator-(const Coord& rhs) const
		{
			return Coord{Value - rhs.Value};
		}
	};

	// A consteval (immediate) member cannot be called from a runtime thunk, so it reflects with no invoker and
	// IsConsteval() stated, the way a consteval constructor does. Reflecting a type that has one must not break
	// the build: without the guard the thunk would call the immediate function at runtime, a hard error.
	struct Immediate
	{
		int Seed = 2;

		consteval int Doubled(int x) const
		{
			return Seed * x;
		}
		int Runtime(int x) const
		{
			return Seed + x;
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

	// Type-hierarchy fixtures. Distinct single-int bases so subobject offsets are deterministic (0, 4, 8).
	struct RootA
	{
		int A = 1;
	};
	struct RootB
	{
		int B = 2;
	};
	struct RootC
	{
		int C = 3;
	};

	// Multiple inheritance with all three access specifiers; RootB/RootC sit at nonzero offsets.
	struct MultiDerived : public RootA, protected RootB, private RootC
	{
		int Own = 0;
	};

	// Two public bases, so a test can independently check the reflected offset against a real upcast.
	struct TwoPublic : public RootA, public RootB
	{
		int Own = 0;
	};

	// A grandchild, to prove reflected bases are direct-only (its only base is MultiDerived, not the roots).
	struct Grandchild : public MultiDerived
	{
		int G = 0;
	};

	// A tag base: it holds no state, so it must contribute nothing to a rendering.
	struct Tag
	{};

	struct TaggedValue : public Tag
	{
		int Value = 5;
	};

	// Everything this type renders comes from the base walk, so it must not render as a fieldless leaf.
	struct InheritedOnly : public RootA
	{};

	// A base that renders itself: the string facet supersedes it, so it stays one named entry rather than
	// being flattened into the derived type's fields.
	struct DerivedFromString : public std::string
	{
		int Tag = 7;
	};

	// Construction fixtures: a value type with default, two-arg, explicit converting, and the implicit
	// copy/move constructors.
	struct Point
	{
		int X = 0;
		int Y = 0;
		Point() = default;
		Point(int x, int y) : X(x), Y(y)
		{}
		explicit Point(int both) : X(both), Y(both)
		{}
	};

	// The classic sink pair: a const-ref and an rvalue-ref overload of the same parameter. Both erase to
	// the same argument tag, and neither is a copy or move constructor of Sinkable itself. Sunk records
	// which overload ran, since a moved-from string's state is unspecified.
	struct Sinkable
	{
		std::string Value;
		bool Sunk = false;

		Sinkable() = default;
		explicit Sinkable(const std::string& value) : Value(value)
		{}
		explicit Sinkable(std::string&& value) : Value(std::move(value)), Sunk(true)
		{}
	};

	// Two overloads separated only by the mutability of their reference. Both bind by reference and neither
	// moves, so nothing an erased argument carries ranks one over the other.
	struct MutabilityOverloads
	{
		int Tag = 0;

		MutabilityOverloads() = default;
		explicit MutabilityOverloads(int& tag) : Tag(tag)
		{}
		explicit MutabilityOverloads(const int& tag) : Tag(tag)
		{}
	};

	// Observes destruction through a pointer it sets in its destructor (so it is not trivially destructible).
	struct Destructible
	{
		bool* Flag = nullptr;
		~Destructible()
		{
			if (Flag)
			{
				*Flag = true;
			}
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

	// Extraction fixtures.

	// A nested namespace, so the scope walk has more than one link to report.
	namespace Deep
	{
		struct Buried
		{
			int Value = 0;
		};
	}

	template <typename T>
	struct Grid
	{
		T Value{};
	};

	// An annotation written on a class template: the language lands it on each instance, which is a type and
	// therefore annotatable, so the template itself never needs to carry one.
	template <typename T>
	struct[[= Doc{"a annotated grid"}]] AnnotatedGrid
	{
		T Value{};
	};

	template <typename T, int N>
	struct FixedArray
	{
		T Data[N];
	};

	template <template <typename> class C, typename T>
	struct Holder
	{
		C<T> Held;
	};

	// Non-type parameters that are not plain values: a reference names an object rather than carrying one,
	// and a pointer's value is the pointer itself.
	inline int SharedSlot = 5;

	template <int& R>
	struct BoundToRef
	{
		int V = 0;
	};

	template <int* P>
	struct BoundToPtr
	{
		int V = 0;
	};

	enum class Mode
	{
		Off,
		On
	};

	template <Mode M>
	struct Configured
	{
		int V = 0;
	};

	template <typename T>
	struct Spec
	{
		T V{};
	};

	template <typename T>
	struct Spec<T*>
	{
		T* V = nullptr;
	};

	struct DefaultPolicy
	{};

	// Stream<Inner> materializes two arguments: the defaulted one is indistinguishable from a written one.
	template <typename T, typename Policy = DefaultPolicy>
	struct Stream
	{
		T V{};
	};

	template <typename T>
	using PtrAlias = T*;

	struct FinalType final
	{
		int V = 0;
	};

	// Static data members spanning both halves of the value/address split. MaxSlots is the link trap: an
	// in-class-initialized static const with no out-of-line definition, whose address cannot be taken.
	struct Registry
	{
		static constexpr int MaxSlots = 8;
		static constexpr double Scale = 2.5;
		static inline int Counter = 0;
		static constexpr std::string Label = "registry";

		int Instance = 1;
	};

	// Every function-level language fact on one type.
	struct Surface
	{
		virtual ~Surface() = default;

		int Plain(int x)
		{
			return x;
		}
		int Constant() const
		{
			return 1;
		}
		static int Stat()
		{
			return 2;
		}
		int Never() const noexcept
		{
			return 3;
		}
		virtual int Virt()
		{
			return 4;
		}
		virtual int Pure() const = 0;
		void Gone() = delete;
		int OnLvalue() &
		{
			return 5;
		}
		int OnRvalue() &&
		{
			return 6;
		}
		int Defaulted(int a, int b = 7)
		{
			return a + b;
		}
		void Qualified(const std::string& byConstRef, std::string&& byRvalueRef)
		{
			(void)byConstRef;
			(void)byRvalueRef;
		}
		const std::string& ByConstRef() const
		{
			return _name;
		}

	protected:
		int Guarded()
		{
			return 8;
		}

	private:
		std::string _name;
	};

	struct SurfaceChild : Surface
	{
		int Virt() override
		{
			return 9;
		}
		int Pure() const override
		{
			return 10;
		}
	};

	// Field-level language facts: a bitfield with a width, a mutable member, a defaulted member, and the
	// three access levels.
	struct Members
	{
		unsigned Small : 3 = 5;
		unsigned Wide : 10 = 300;
		int Plain;
		int WithDefault = 42;
		mutable int Cached = 0;

	protected:
		int Guarded = 0;

	private:
		int Hidden = 0;
	};

	// A volatile-qualified data member (a memory-mapped register), told apart from its unqualified tag only by
	// IsVolatile, the way const and reference members are.
	struct VolatileHolder
	{
		volatile int Register = 0;
		int Plain = 0;
	};

	// A volatile class-typed member: no copy constructor binds a volatile lvalue and no assignment operator
	// takes one, so it reflects with neither getter nor setter rather than emitting a body that cannot compile.
	// The scalar case above is readable and writable; only the class-typed one loses both.
	struct VolatileClassHolder
	{
		struct Payload
		{
			int A = 0;
		};

		volatile Payload Item;
		int Plain = 0;
	};

	// An anonymous nested struct (the shape of `struct { int a; } field;`) is a type member with no identifier,
	// so it is excluded from GetNestedTypes: nothing can name it, and its field already reaches it.
	struct AnonymousNested
	{
		struct
		{
			int A = 0;
		} Unnamed;

		struct Named
		{
			int B = 0;
		};

		int Plain = 0;
	};

	// A defaulted operator==: the language generates memberwise comparison, so IsDefaulted() is true on the
	// reflected operator, unlike Coord's hand-written one.
	struct Comparable
	{
		int Value = 0;
		bool operator==(const Comparable&) const = default;
	};

	// A defaulted destructor on a non-trivial type: IsDefaulted() true, IsTrivial() false (the string member
	// makes destruction non-trivial).
	struct DefaultedDestructor
	{
		std::string Name;
		~DefaultedDestructor() = default;
	};

	// A deleted destructor: reflects with no destroy thunk (CanDestroy() false) and IsDeleted() stated.
	struct DeletedDestructor
	{
		int Value = 0;
		~DeletedDestructor() = delete;
	};

	// Nested types and member type-aliases, the reflexive structure GetNestedTypes() reports: Config and Season
	// are real nested definitions, ValueAlias names a fundamental, and SelfAlias names the enclosing type.
	struct NestedOwner
	{
		struct Config
		{
			int Width = 0;
		};
		enum class Season
		{
			Winter,
			Summer
		};
		using ValueAlias = int;
		using SelfAlias = NestedOwner;

		Config Current;
		int Plain = 0;
	};

	// ReSharper restore CppMemberFunctionMayBeConst
	// ReSharper restore CppMemberFunctionMayBeStatic
	// ReSharper restore CppParameterMayBeConst
	// ReSharper restore CppDeclaratorNeverUsed
	// ReSharper restore CppPassValueParameterByConstReference
	// ReSharper restore CppEnumeratorNeverUsed
}

// Not exported, so it has module linkage rather than external: no cross-translation-unit identity. A
// non-exported name is not visible to an importer, so an exported alias is what lets a test name it; the
// alias does not change the linkage of the type it names.
namespace ReflectionTestTypes
{
	struct ModuleLocal
	{
		int Value = 0;
	};

	export using ModuleLocalAlias = ModuleLocal;
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
