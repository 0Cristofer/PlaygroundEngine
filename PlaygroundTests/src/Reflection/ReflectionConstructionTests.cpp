#include <doctest/doctest.h>

import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

import std;

using namespace ReflectionTestTypes;

TEST_CASE("a type exposes its constructors classified by kind")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	// Default, the explicit converting (int), the two-arg (Other), plus the implicit copy and move.
	CHECK(point.FindConstructor(PgE::ConstructorKind::Default) != nullptr);
	CHECK(point.FindConstructor(PgE::ConstructorKind::Converting) != nullptr);
	CHECK(point.FindConstructor(PgE::ConstructorKind::Other) != nullptr);
	CHECK(point.FindConstructor(PgE::ConstructorKind::Copy) != nullptr);
	CHECK(point.FindConstructor(PgE::ConstructorKind::Move) != nullptr);
}

TEST_CASE("the explicit flag is reflected on constructors")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	// Point(int) is explicit; the two-arg and default constructors are not.
	CHECK(point.FindConstructor(PgE::ConstructorKind::Converting)->IsExplicit());
	CHECK_FALSE(point.FindConstructor(PgE::ConstructorKind::Other)->IsExplicit());
	CHECK_FALSE(point.FindConstructor(PgE::ConstructorKind::Default)->IsExplicit());
}

TEST_CASE("default construction produces a defaulted value")
{
	const PgE::ConstructorInfo* defaultConstructor = PgE::TypeOf<Point>().FindConstructor(PgE::ConstructorKind::Default);
	REQUIRE(defaultConstructor->CanConstruct());

	const auto made = defaultConstructor->ConstructAs<Point>();
	REQUIRE(made.has_value());
	CHECK(made->X == 0);
	CHECK(made->Y == 0);
}

TEST_CASE("parameterized construction binds erased arguments to the right constructor")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	const auto pair = point.FindConstructor(PgE::ConstructorKind::Other)->ConstructAs<Point>(3, 4);
	REQUIRE(pair.has_value());
	CHECK(pair->X == 3);
	CHECK(pair->Y == 4);

	const auto both = point.FindConstructor(PgE::ConstructorKind::Converting)->ConstructAs<Point>(5);
	REQUIRE(both.has_value());
	CHECK(both->X == 5);
	CHECK(both->Y == 5);
}

TEST_CASE("copy and move construction go through the constructor thunks")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();
	Point source{7, 9};

	const auto copied = point.FindConstructor(PgE::ConstructorKind::Copy)->ConstructAs<Point>(source);
	REQUIRE(copied.has_value());
	CHECK(copied->X == 7);
	CHECK(copied->Y == 9);

	const auto moved = point.FindConstructor(PgE::ConstructorKind::Move)->ConstructAs<Point>(Point{1, 2});
	REQUIRE(moved.has_value());
	CHECK(moved->X == 1);
	CHECK(moved->Y == 2);
}

TEST_CASE("a move-only type reflects a usable move constructor and an unusable copy constructor")
{
	const PgE::TypeInfo& moveOnly = PgE::TypeOf<MoveOnly>();

	const PgE::ConstructorInfo* moveConstructor = moveOnly.FindConstructor(PgE::ConstructorKind::Move);
	REQUIRE(moveConstructor != nullptr);
	CHECK(moveConstructor->CanConstruct());

	MoveOnly source;
	source.Tag = 42;
	const auto moved = moveConstructor->ConstructAs<MoveOnly>(std::move(source));
	REQUIRE(moved.has_value());
	CHECK(moved->Tag == 42);

	// The deleted copy constructor, if reflected at all, carries no thunk.
	const PgE::ConstructorInfo* copyConstructor = moveOnly.FindConstructor(PgE::ConstructorKind::Copy);
	if (copyConstructor != nullptr)
	{
		CHECK_FALSE(copyConstructor->CanConstruct());
	}
}

TEST_CASE("a type constructs from arguments without the caller naming a constructor")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	const auto defaulted = point.ConstructAs<Point>();
	REQUIRE(defaulted.has_value());
	CHECK(defaulted->X == 0);

	const auto pair = point.ConstructAs<Point>(3, 4);
	REQUIRE(pair.has_value());
	CHECK(pair->X == 3);
	CHECK(pair->Y == 4);

	// One int selects the converting constructor, which fills both fields.
	const auto both = point.ConstructAs<Point>(5);
	REQUIRE(both.has_value());
	CHECK(both->X == 5);
	CHECK(both->Y == 5);
}

TEST_CASE("argument value category selects between the copy and move constructors")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();
	Point source{7, 9};

	// Copy and move erase to the same argument tag, so Movable is the only thing that separates them.
	const std::array borrowed{PgE::TypedRef{.Type = &point, .Data = &source, .IsConst = false, .Movable = false}};
	const std::array offered{PgE::TypedRef{.Type = &point, .Data = &source, .IsConst = false, .Movable = true}};
	const std::array constant{PgE::TypedRef{.Type = &point, .Data = &source, .IsConst = true, .Movable = true}};

	CHECK(point.FindConstructor(borrowed)->GetKind() == PgE::ConstructorKind::Copy);
	CHECK(point.FindConstructor(offered)->GetKind() == PgE::ConstructorKind::Move);

	// A const argument cannot be moved from however the caller tagged it.
	CHECK(point.FindConstructor(constant)->GetKind() == PgE::ConstructorKind::Copy);
}

TEST_CASE("the typed sugar offers only an rvalue argument to the move constructor")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();
	Point source{7, 9};

	const auto copied = point.ConstructAs<Point>(source);
	REQUIRE(copied.has_value());
	CHECK(copied->X == 7);

	const auto moved = point.ConstructAs<Point>(Point{1, 2});
	REQUIRE(moved.has_value());
	CHECK(moved->X == 1);
	CHECK(moved->Y == 2);
}

TEST_CASE("a move-only type constructs from an offered argument but never from a borrowed one")
{
	const PgE::TypeInfo& moveOnly = PgE::TypeOf<MoveOnly>();
	MoveOnly source;
	source.Tag = 42;

	// The deleted copy constructor carries no thunk, so it is not a candidate and the move constructor is
	// the only match. It still refuses the lvalue rather than silently moving out of the caller's object.
	const auto borrowed = moveOnly.ConstructAs<MoveOnly>(source);
	REQUIRE_FALSE(borrowed.has_value());
	CHECK(borrowed.error().Reason == PgE::ConstructError::NotMovable);
	CHECK(source.Tag == 42);

	const auto moved = moveOnly.ConstructAs<MoveOnly>(std::move(source));
	REQUIRE(moved.has_value());
	CHECK(moved->Tag == 42);
}

TEST_CASE("a const-ref and rvalue-ref overload pair is separated by the argument's value category")
{
	const PgE::TypeInfo& sinkable = PgE::TypeOf<Sinkable>();
	std::string value = "hello";

	// Neither overload is a copy or move constructor of Sinkable, so only the parameters' own binding
	// separates them: both take std::string and both carry a thunk.
	const auto borrowed = sinkable.ConstructAs<Sinkable>(value);
	REQUIRE(borrowed.has_value());
	CHECK_FALSE(borrowed->Sunk);
	CHECK(borrowed->Value == "hello");
	CHECK(value == "hello");

	const auto offered = sinkable.ConstructAs<Sinkable>(std::move(value));
	REQUIRE(offered.has_value());
	CHECK(offered->Sunk);
	CHECK(offered->Value == "hello");
}

TEST_CASE("a const argument reaches the const-ref overload even when tagged movable")
{
	const PgE::TypeInfo& sinkable = PgE::TypeOf<Sinkable>();
	std::string value = "hello";

	// Movable is the caller's offer, but a const argument cannot be moved out of, so the offer is void.
	const std::array constant{PgE::TypedRef{.Type = &PgE::TypeOf<std::string>(), .Data = &value, .IsConst = true, .Movable = true}};
	const PgE::ConstructorInfo* selected = sinkable.FindConstructor(constant);
	REQUIRE(selected != nullptr);
	CHECK_FALSE(selected->GetParams()[0].BindsByMove());
}

TEST_CASE("overloads the erased arguments cannot rank are reported as ambiguous")
{
	const PgE::TypeInfo& overloads = PgE::TypeOf<MutabilityOverloads>();
	int tag = 7;

	// int& and const int& erase to the same tag and both bind by reference, so neither is the better match.
	const std::array args{PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = &tag, .IsConst = false, .Movable = false}};
	const auto ambiguous = overloads.ConstructAs<MutabilityOverloads>(tag);
	REQUIRE_FALSE(ambiguous.has_value());
	CHECK(ambiguous.error().Reason == PgE::ConstructError::AmbiguousConstructor);
	CHECK(overloads.FindConstructor(args) == nullptr);
}

TEST_CASE("an argument with no object is rejected before it is bound")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	int y = 2;
	const std::array args{PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = nullptr}, PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = &y}};

	alignas(Point) std::byte storage[sizeof(Point)];
	const PgE::TypedRef slot{.Type = &point, .Data = storage, .IsConst = false};

	const auto rejected = point.FindConstructor(PgE::ConstructorKind::Other)->Construct(args, slot);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::ConstructError::NullArgument);
	CHECK(rejected.error().ArgumentIndex == 0);
}

TEST_CASE("construction from arguments matching no constructor reports no match")
{
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	// Right arity, wrong argument types: the binder admits no conversions, so double does not reach Point(int, int).
	const auto wrongTypes = point.ConstructAs<Point>(3.0, 4.0);
	REQUIRE_FALSE(wrongTypes.has_value());
	CHECK(wrongTypes.error().Reason == PgE::ConstructError::NoMatchingConstructor);

	const auto wrongArity = point.ConstructAs<Point>(1, 2, 3);
	REQUIRE_FALSE(wrongArity.has_value());
	CHECK(wrongArity.error().Reason == PgE::ConstructError::NoMatchingConstructor);

	double value = 3.0;
	const std::array wrongType{PgE::TypedRef{.Type = &PgE::TypeOf<double>(), .Data = &value}};
	CHECK(point.FindConstructor(wrongType) == nullptr);
}

TEST_CASE("construction validates arity, argument type, and the destination slot")
{
	const PgE::ConstructorInfo* twoArg = PgE::TypeOf<Point>().FindConstructor(PgE::ConstructorKind::Other);

	int x = 1;
	int y = 2;
	std::array goodArgs{PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = &x}, PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = &y}};

	alignas(Point) std::byte storage[sizeof(Point)];
	const PgE::TypedRef slot{.Type = &PgE::TypeOf<Point>(), .Data = storage, .IsConst = false};

	// Wrong arity: one argument for a two-argument constructor.
	const auto arity = twoArg->Construct(std::span{goodArgs}.first(1), slot);
	REQUIRE_FALSE(arity.has_value());
	CHECK(arity.error().Reason == PgE::ConstructError::ArityMismatch);

	// Wrong argument type: a double where an int is expected.
	double wrong = 3.0;
	std::array badArgs{PgE::TypedRef{.Type = &PgE::TypeOf<double>(), .Data = &wrong}, PgE::TypedRef{.Type = &PgE::TypeOf<int>(), .Data = &y}};
	const auto typeError = twoArg->Construct(badArgs, slot);
	REQUIRE_FALSE(typeError.has_value());
	CHECK(typeError.error().Reason == PgE::ConstructError::TypeMismatch);
	CHECK(typeError.error().ArgumentIndex == 0);

	// Wrong slot: destination tagged as a different type than the one being constructed.
	int wrongSlotStorage = 0;
	const PgE::TypedRef wrongSlot{.Type = &PgE::TypeOf<int>(), .Data = &wrongSlotStorage, .IsConst = false};
	const auto slotError = twoArg->Construct(goodArgs, wrongSlot);
	REQUIRE_FALSE(slotError.has_value());
	CHECK(slotError.error().Reason == PgE::ConstructError::SlotTypeMismatch);

	// A valid call still constructs into the slot.
	const auto ok = twoArg->Construct(goodArgs, slot);
	REQUIRE(ok.has_value());
	Point* constructed = std::launder(reinterpret_cast<Point*>(storage));
	CHECK(constructed->X == 1);
	CHECK(constructed->Y == 2);
	std::destroy_at(constructed);
}

TEST_CASE("a type carries a destroy thunk that runs its destructor")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Destructible>();
	REQUIRE(type.CanDestroy());

	bool destroyed = false;
	alignas(Destructible) std::byte storage[sizeof(Destructible)];
	auto object = ::new (storage) Destructible{};
	object->Flag = &destroyed;

	type.Destroy(object);
	CHECK(destroyed);
}

TEST_CASE("a fundamental type reports it can be destroyed but has no constructor list")
{
	const PgE::TypeInfo& intType = PgE::TypeOf<int>();
	CHECK(intType.CanDestroy());
	CHECK(intType.GetConstructors().empty());
}
