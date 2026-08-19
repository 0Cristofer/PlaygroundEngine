#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

TEST_CASE("reflected member function invokes and returns")
{
	Widget widget{3, 4};

	const std::vector<const PgE::FunctionInfo*> areas = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Area");
	REQUIRE(areas.size() == 1);

	const auto result = areas.front()->InvokeAs<int>(widget);

	REQUIRE(result.has_value());
	CHECK(*result == 12);
}

TEST_CASE("reflected member function mutates its instance")
{
	Widget widget{1, 1};

	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();
	const auto result = resize->InvokeAs(widget, 2, 5);

	REQUIRE(result.has_value());
	CHECK(widget.Width == 2);
	CHECK(widget.Height == 5);
}

TEST_CASE("invoke reports arity and type mismatches")
{
	Widget widget{};
	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	int width = 2;
	const PgE::TypedRef tooFew[] = {{.Type = &PgE::TypeOf<int>(), .Data = &width, .IsConst = false}};
	const auto arity = resize->Invoke(PgE::TypedRefOf(widget), tooFew);
	REQUIRE_FALSE(arity.has_value());
	CHECK(arity.error().Reason == PgE::InvokeError::ArityMismatch);

	int height = 5;
	std::string wrong = "x";
	const PgE::TypedRef mistyped[] = {{.Type = &PgE::TypeOf<std::string>(), .Data = &wrong, .IsConst = false},
									  {.Type = &PgE::TypeOf<int>(), .Data = &height, .IsConst = false}};
	const auto type = resize->Invoke(PgE::TypedRefOf(widget), mistyped);
	REQUIRE_FALSE(type.has_value());
	CHECK(type.error().Reason == PgE::InvokeError::TypeMismatch);
	CHECK(type.error().ArgumentIndex == 0);

	// Data is the address of the argument object, so a null one names no object at all. Unlike the return
	// slot, where null is the caller discarding the result, there is nothing an argument could mean by it.
	const PgE::TypedRef noObject[] = {{.Type = &PgE::TypeOf<int>(), .Data = &width, .IsConst = false},
									  {.Type = &PgE::TypeOf<int>(), .Data = nullptr, .IsConst = false}};
	const auto null = resize->Invoke(PgE::TypedRefOf(widget), noObject);
	REQUIRE_FALSE(null.has_value());
	CHECK(null.error().Reason == PgE::InvokeError::NullArgument);
	CHECK(null.error().ArgumentIndex == 1);
}

// The counterpart to NullArgument above: for a pointer parameter the argument object is the pointer, so
// null is an ordinary value reaching the call rather than a missing argument.
TEST_CASE("a pointer parameter takes a null pointer as a value")
{
	PointerSink sink{};
	const PgE::FunctionInfo* aim = PgE::TypeOf<PointerSink>().FindFunctionsByIdentifier("Aim").front();

	int target = 3;
	int* address = &target;
	const PgE::TypedRef pointsAtTarget[] = {{.Type = &PgE::TypeOf<int*>(), .Data = &address, .IsConst = false}};
	REQUIRE(aim->Invoke(PgE::TypedRefOf(sink), pointsAtTarget).has_value());
	CHECK(sink.Received == &target);

	// Data is the address of the pointer, which holds null; the call must land with Received back to null.
	sink = PointerSink{};
	int* nothing = nullptr;
	const PgE::TypedRef pointsAtNothing[] = {{.Type = &PgE::TypeOf<int*>(), .Data = &nothing, .IsConst = false}};
	REQUIRE(aim->Invoke(PgE::TypedRefOf(sink), pointsAtNothing).has_value());
	CHECK(sink.Called);
	CHECK(sink.Received == nullptr);
}

// All three pointer parameter forms erase to the same int* tag (remove_cvref collapses them), so the tag
// cannot be what separates them: int*& is set apart by demanding a mutable argument it can write through.
TEST_CASE("a reference-to-pointer parameter binds the caller's own pointer")
{
	PointerSink sink{};
	const PgE::FunctionInfo* retarget = PgE::TypeOf<PointerSink>().FindFunctionsByIdentifier("Retarget").front();

	int* address = nullptr;
	const PgE::TypedRef mutablePointer[] = {{.Type = &PgE::TypeOf<int*>(), .Data = &address, .IsConst = false}};
	REQUIRE(retarget->Invoke(PgE::TypedRefOf(sink), mutablePointer).has_value());
	CHECK(address == &sink.Owned);

	const PgE::TypedRef constPointer[] = {{.Type = &PgE::TypeOf<int*>(), .Data = &address, .IsConst = true}};
	const auto rejected = retarget->Invoke(PgE::TypedRefOf(sink), constPointer);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::ConstViolation);
}

TEST_CASE("a const-reference-to-pointer parameter takes a null pointer as a value")
{
	PointerSink sink{};
	const PgE::FunctionInfo* peek = PgE::TypeOf<PointerSink>().FindFunctionsByIdentifier("Peek").front();

	int* nothing = nullptr;
	const PgE::TypedRef pointsAtNothing[] = {{.Type = &PgE::TypeOf<int*>(), .Data = &nothing, .IsConst = true}};
	REQUIRE(peek->Invoke(PgE::TypedRefOf(sink), pointsAtNothing).has_value());
	CHECK(sink.Called);
	CHECK(sink.Received == nullptr);
}

TEST_CASE("const object reaches only const-callable functions")
{
	Counter counter{7};
	const Counter& readOnly = counter;
	const PgE::TypeInfo& type = PgE::TypeOf<Counter>();

	const auto value = type.FindFunctionsByIdentifier("Get").front()->InvokeAs<int>(readOnly);
	REQUIRE(value.has_value());
	CHECK(*value == 7);

	const auto mutated = type.FindFunctionsByIdentifier("Add").front()->InvokeAs(readOnly, 1);
	REQUIRE_FALSE(mutated.has_value());
	CHECK(mutated.error().Reason == PgE::InvokeError::ConstViolation);
	CHECK(counter.Value == 7);
}

TEST_CASE("reflected function returning a reference yields a live reference")
{
	Accessor accessor{};

	const auto mutableRef = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Mutable").front()->InvokeAs<int&>(accessor);
	REQUIRE(mutableRef.has_value());
	int& reference = mutableRef.value();
	reference = 42;
	CHECK(accessor.Value == 42);

	const auto view = PgE::TypeOf<Accessor>().FindFunctionsByIdentifier("Readonly").front()->InvokeAs<const int&>(accessor);
	REQUIRE(view.has_value());
	CHECK(view.value() == 42);
}

TEST_CASE("value-return sugar on a void function reports ReturnTypeMismatch")
{
	Widget widget{};
	const PgE::FunctionInfo* resize = PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	const auto result = resize->InvokeAs<int>(widget, 2, 5);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().Reason == PgE::InvokeError::ReturnTypeMismatch);
	CHECK(widget.Width == 0);
}

TEST_CASE("a move-only parameter takes only an argument the caller hands over")
{
	Sink sink{};
	const PgE::FunctionInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();

	MoveOnly readOnlyArg;
	const PgE::TypedRef constArg[] = {{&PgE::TypeOf<MoveOnly>(), &readOnlyArg, true}};
	const auto rejected = consume->Invoke(PgE::TypedRefOf(sink), constArg);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::ConstViolation);

	// Consume(MoveOnly) can only move out of its argument, so a mutable but merely borrowed argument is
	// refused too: being non-const is not the caller offering the object up.
	MoveOnly borrowedArg;
	const PgE::TypedRef borrowed[] = {{.Type = &PgE::TypeOf<MoveOnly>(), .Data = &borrowedArg, .IsConst = false, .Movable = false}};
	const auto refused = consume->Invoke(PgE::TypedRefOf(sink), borrowed);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error().Reason == PgE::InvokeError::NotMovable);

	MoveOnly ownedArg;
	ownedArg.Tag = 9;
	const PgE::TypedRef offered[] = {{.Type = &PgE::TypeOf<MoveOnly>(), .Data = &ownedArg, .IsConst = false, .Movable = true}};
	const auto accepted = consume->Invoke(PgE::TypedRefOf(sink), offered);
	REQUIRE(accepted.has_value());
	CHECK(sink.Value == 9);
}

TEST_CASE("reflection invokes private member functions")
{
	const PgE::TypeInfo& type = PgE::TypeOf<WithPrivate>();
	WithPrivate obj{21};

	const PgE::FunctionInfo* doubled = type.FindFunctionsByIdentifier("Doubled").front();
	const auto read = doubled->InvokeAs<int>(obj);
	REQUIRE(read.has_value());
	CHECK(*read == 42);

	const PgE::FunctionInfo* setValue = type.FindFunctionsByIdentifier("SetValue").front();
	REQUIRE(setValue->InvokeAs(obj, 5).has_value());
	CHECK(obj.Value == 5);
}

TEST_CASE("a function the thunk cannot call reflects as metadata but is not invocable")
{
	const PgE::TypeInfo& type = PgE::TypeOf<RefQualified>();
	RefQualified obj{7};

	// The rvalue-ref-qualified overload is reflected, but invoking it reports NotInvocable rather than
	// failing to compile or crashing on a null thunk.
	const PgE::FunctionInfo* onRvalue = type.FindFunctionsByIdentifier("OnRvalue").front();
	const auto rejected = onRvalue->InvokeAs<int>(obj);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::NotInvocable);

	// A normal overload on the same type still invokes.
	CHECK(type.FindFunctionsByIdentifier("OnAny").front()->InvokeAs<int>(obj).value() == 7);
}

TEST_CASE("a deducing-this member drops the object parameter and reads its qualifiers off it")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Deducing>();
	Deducing obj{.Base = 100};

	const PgE::FunctionInfo* get = type.FindFunctionsByIdentifier("Get").front();

	// The explicit object parameter is not a caller argument, so the reflected arity is 1 (add), not 2,
	// and the fact is stated rather than left to be inferred from a miscount.
	CHECK(get->HasExplicitObjectParameter());
	CHECK(get->GetParams().size() == 1);
	CHECK_FALSE(get->IsStatic());

	// const-ness is read off the object parameter (const Deducing&), not the function, so it is const-callable.
	CHECK(get->IsConst());
	CHECK(get->IsConstCallable());
	CHECK(get->InvokeAs<int>(obj, 5).value() == 105);

	const Deducing& readOnly = obj;
	CHECK(get->InvokeAs<int>(readOnly, 5).value() == 105);

	// A mutable object parameter (Deducing&) is not const-callable: invoking through a const object is
	// rejected, exactly as an ordinary non-const member would be.
	const PgE::FunctionInfo* bump = type.FindFunctionsByIdentifier("Bump").front();
	CHECK_FALSE(bump->IsConst());
	CHECK(bump->HasExplicitObjectParameter());
	CHECK(bump->InvokeAs<int>(obj, 3).value() == 103);

	const auto onConst = bump->InvokeAs<int>(readOnly, 1);
	REQUIRE_FALSE(onConst.has_value());
	CHECK(onConst.error().Reason == PgE::InvokeError::ConstViolation);

	// A by-value object parameter copies the object, so the call cannot mutate the caller's object: it is
	// const-callable, matching what the language allows (calling it on a const object is well-formed).
	const PgE::FunctionInfo* copy = type.FindFunctionsByIdentifier("Copy").front();
	CHECK(copy->IsConst());
	CHECK(copy->IsConstCallable());
	CHECK(copy->GetRefQualifier() == PgE::RefQualifier::None);

	// Bump above mutated Base to 103, and readOnly views the same object; Copy leaves it untouched.
	CHECK(copy->InvokeAs<int>(readOnly, 4).value() == 107);
}

TEST_CASE("a consteval member function reflects as metadata but is not invocable")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Immediate>();
	Immediate obj{5};

	// The immediate function cannot be called from a runtime thunk, so it has no invoker; the metadata states
	// why (IsConsteval), and reflecting the type does not break the build.
	const PgE::FunctionInfo* doubled = type.FindFunctionsByIdentifier("Doubled").front();
	CHECK(doubled->IsConsteval());
	const auto rejected = doubled->InvokeAs<int>(obj, 3);
	REQUIRE_FALSE(rejected.has_value());
	CHECK(rejected.error().Reason == PgE::InvokeError::NotInvocable);

	// A normal member on the same type still invokes and is not marked consteval.
	const PgE::FunctionInfo* runtime = type.FindFunctionsByIdentifier("Runtime").front();
	CHECK_FALSE(runtime->IsConsteval());
	CHECK(runtime->InvokeAs<int>(obj, 3).value() == 8);
}

TEST_CASE("a private deducing-this member invokes through the pointer route")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Deducing>();
	Deducing obj{.Base = 6};

	const PgE::FunctionInfo* secret = type.FindFunctionsByIdentifier("Secret").front();
	CHECK(secret->HasExplicitObjectParameter());
	CHECK(secret->GetAccess() == PgE::AccessKind::Private);
	CHECK(secret->InvokeAs<int>(obj, 4).value() == 24);
}

TEST_CASE("invoking through a base type calls the overwritten function")
{
	Child child{};
	ChildNoOverride childNoOverride{};
	const PgE::TypeInfo& baseType = PgE::TypeOf<Base>();
	const PgE::TypeInfo& childType = PgE::TypeOf<Child>();
	const PgE::TypeInfo& childNoOverrideType = PgE::TypeOf<ChildNoOverride>();

	auto baseFunctions = baseType.FindFunctionsByIdentifier("GetV");
	auto childFunctions = childType.FindFunctionsByIdentifier("GetV");
	auto childNoOverrideFunctions = childNoOverrideType.FindFunctionsByIdentifier("GetV");

	REQUIRE(baseFunctions.size() == 1);
	REQUIRE(childFunctions.size() == 1);
	REQUIRE(childNoOverrideFunctions.size() == 0);
	// The base's invoker is built against Base, so it takes the base subobject: the derived object is
	// upcast first, and the tag on the borrow is what makes that a checked step rather than an assumed one.
	const PgE::TypedRef childAsBase = childType.GetBases()[0].Upcast(PgE::TypedRefOf(child));
	const PgE::TypedRef noOverrideAsBase = childNoOverrideType.GetBases()[0].Upcast(PgE::TypedRefOf(childNoOverride));

	REQUIRE(baseFunctions.front()->InvokeAs<int>(childAsBase).has_value());
	REQUIRE(baseFunctions.front()->InvokeAs<int>(noOverrideAsBase).has_value());
	REQUIRE(childFunctions.front()->InvokeAs<int>(child).has_value());

	// Virtual dispatch still runs off the object, so the override wins even through the base's invoker.
	CHECK(baseFunctions.front()->InvokeAs<int>(childAsBase).value() == 2);
	CHECK(baseFunctions.front()->InvokeAs<int>(noOverrideAsBase).value() == 1);
	CHECK(childFunctions.front()->InvokeAs<int>(child).value() == 2);

	// Handing the derived object straight to the base's invoker is the mistake the tag exists to catch.
	CHECK(baseFunctions.front()->InvokeAs<int>(child).error().Reason == PgE::InvokeError::ObjectTypeMismatch);
}

TEST_CASE("invoke moves a by-value argument only for rvalues")
{
	Consumer consumer{};
	const PgE::FunctionInfo* store = PgE::TypeOf<Consumer>().FindFunctionsByIdentifier("Store").front();

	Tracked byCopy;
	byCopy.Value = 3;
	REQUIRE(store->InvokeAs(consumer, byCopy).has_value());
	CHECK(consumer.Stored == 3);
	CHECK_FALSE(byCopy.Moved);

	Tracked byMove;
	byMove.Value = 8;
	REQUIRE(store->InvokeAs(consumer, std::move(byMove)).has_value());
	CHECK(consumer.Stored == 8);
	CHECK(byMove.Moved);
}

TEST_CASE("InvokeAs moves a move-only argument into a parameter")
{
	Sink sink{};
	MoveOnly item;
	item.Tag = 4;

	const PgE::FunctionInfo* consume = PgE::TypeOf<Sink>().FindFunctionsByIdentifier("Consume").front();
	REQUIRE(consume->InvokeAs(sink, std::move(item)).has_value());
	CHECK(sink.Value == 4);
}

TEST_CASE("invoke copies a by-value argument with a deleted move constructor")
{
	CopyConsumer consumer{};
	CopyOnlyParam argument;
	argument.Value = 6;

	const PgE::FunctionInfo* take = PgE::TypeOf<CopyConsumer>().FindFunctionsByIdentifier("Take").front();
	REQUIRE(take->InvokeAs(consumer, argument).has_value());
	CHECK(consumer.Stored == 6);
}

TEST_CASE("a reached type with a deprecated member reflects as metadata without reifying its members")
{
	// The metadata handle never reflects member functions (the list is built only at demand, through
	// TypeOf<T>), so reaching a type as metadata reifies no member and cannot break on a deprecated or
	// ill-formed one. Naming this type through TypeOf<T> would splice its deprecated member and fail to build.
	const PgE::TypeInfo& holder = PgE::TypeMetaOf<DeprecatedMemberHolder>();
	CHECK(holder.GetFunctions().empty());

	// ToString walks the owner's fields by offset and recurses into the holder as metadata, so rendering a
	// type that transitively contains a deprecated member is total, the way it must be for a foreign vk struct.
	CHECK(PgE::ToString(HolderOwner{}) == "{Width: 800, Holder: {Severity: 0}}");
}

TEST_CASE("a transitively reached type with a constexpr member ill-formed for its argument reflects as metadata")
{
	// copy() is ill-formed for T = OpaqueElement, so the metadata walk must never reify the wrapper's member
	// functions, or instantiating copy would hard-error. The list is empty until the type is named for
	// invocation, which nothing here does, so reaching it only as metadata keeps the build total.
	const PgE::TypeInfo& wrapper = PgE::TypeMetaOf<ConstexprMemberWrapper<OpaqueElement, 2>>();
	CHECK(wrapper.GetFunctions().empty());

	// ToString recurses into the wrapper as metadata; the fact that this compiles at all is the regression guard.
	CHECK(PgE::ToString(WrapperOwner{}).starts_with("{Count: 3, Wrapper:"));
}

TEST_CASE("AreOpsMaterialized distinguishes a pending empty function list from a genuinely empty one")
{
	// DeprecatedMemberHolder is never named through TypeOf (its deprecated member would break the build), so its
	// op-lists stay empty because they are pending, not because it has no functions.
	CHECK_FALSE(PgE::TypeMetaOf<DeprecatedMemberHolder>().AreOpsMaterialized());
	CHECK(PgE::TypeMetaOf<DeprecatedMemberHolder>().GetFunctions().empty());

	// NoFunctions is splice-able, so naming it through TypeOf materializes its ops; its function list is then
	// empty because it genuinely has none, which AreOpsMaterialized reports true against the pending case above.
	const PgE::TypeInfo& materialized = PgE::TypeOf<NoFunctions>();
	CHECK(materialized.AreOpsMaterialized());
	CHECK(materialized.GetFunctions().empty());
}

TEST_CASE("a member call states what it needs of the object")
{
	Widget widget{.Width = 2, .Height = 3};
	const PgE::FunctionInfo& resize = *PgE::TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();

	REQUIRE_FALSE(resize.CallsWithoutObject());
	REQUIRE(resize.InvokeAs(widget, 4, 5).has_value());

	// No object at all, and a null one, are the same answer: this call cannot run without one.
	CHECK(resize.InvokeStaticAs(4, 5).error().Reason == PgE::InvokeError::ObjectRequired);
	CHECK(resize.InvokeAs(PgE::TypedRef{}, 4, 5).error().Reason == PgE::InvokeError::ObjectRequired);

	// A wrong-typed object would run the invoker against another layout, which is what the tag stops.
	Coord coord{};
	CHECK(resize.InvokeAs(coord, 4, 5).error().Reason == PgE::InvokeError::ObjectTypeMismatch);
	CHECK(widget.Width == 4);
}

TEST_CASE("a virtual call through a base at a nonzero offset dispatches to the override")
{
	TwoVoices voices;
	const std::span<const PgE::BaseInfo> bases = PgE::TypeOf<TwoVoices>().GetBases();
	REQUIRE(bases.size() == 2);

	// The point of the fixture: an upcast that forgot to add the offset would still pass at index 0.
	REQUIRE(bases[0].GetOffset() == 0);
	REQUIRE(bases[1].GetOffset() != 0);

	const PgE::FunctionInfo& answer = *PgE::TypeOf<SecondVoice>().FindFunctionsByIdentifier("Answer").front();
	const PgE::TypedRef asSecondVoice = bases[1].Upcast(PgE::TypedRefOf(voices));
	REQUIRE(asSecondVoice.Data == static_cast<SecondVoice*>(&voices));

	// Through the right subobject the vptr is SecondVoice's, so the override runs.
	CHECK(answer.InvokeAs<int>(asSecondVoice).value() == 20);

	// The derived address points at FirstVoice's vptr, so this would dispatch through the wrong table.
	CHECK(answer.InvokeAs<int>(voices).error().Reason == PgE::InvokeError::ObjectTypeMismatch);

	// The first base needs no adjustment, and must still reach its own override.
	const PgE::FunctionInfo& speak = *PgE::TypeOf<FirstVoice>().FindFunctionsByIdentifier("Speak").front();
	CHECK(speak.InvokeAs<int>(bases[0].Upcast(PgE::TypedRefOf(voices))).value() == 2);

	// One name, two overloads: a borrow is invoked on directly, a typed object erases itself first. A
	// TypedRef must never take the deduced path, which would tag the call with TypedRef and match nothing.
	PgE::TypedRef borrow = bases[1].Upcast(PgE::TypedRefOf(voices));
	CHECK(answer.InvokeAs<int>(borrow).value() == 20);
	CHECK(answer.InvokeAs<int>(std::as_const(borrow)).value() == 20);
}
