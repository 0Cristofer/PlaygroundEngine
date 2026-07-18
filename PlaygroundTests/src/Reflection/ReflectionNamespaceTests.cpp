#include <doctest/doctest.h>
#include <meta>

import std;
import PlaygroundEngine.Reflection;

// The fixtures are declared in this translation unit rather than in the shared test-types module on purpose:
// a sweep answers for what the asking TU has seen, so a fixture whose visibility depends on an import order
// would make the expected counts a property of the build. See docs/ReflectionExtraction.md (namespace sweep).

namespace
{
	struct Marker
	{
		int Id = 0;
	};
}

namespace[[= Marker{.Id = 7}]] SweepFixture
{
	struct Widget
	{
		int Value = 0;
	};

	enum class Color
	{
		Red
	};

	using WidgetAlias = Widget;

	int Overloaded(const int count)
	{
		return count;
	}

	void Overloaded()
	{}

	int Unique(const int value)
	{
		return value * 2;
	}

	int Counter = 7;
	constexpr int MaxSlots = 8;
	thread_local int PerThread = 5;

	// GCC 17 crashes building metadata for a reference variable, so the sweep omits it. Declared here so the
	// omission is exercised: without it, a namespace holding one would fail to compile at all.
	int& ReferenceVariable = Counter;

	template <typename T>
	struct Grid
	{};

	template <typename T>
	void Generic(T)
	{}

	template <typename T>
	concept Constrained = true;

	namespace Inner
	{
		struct Deep
		{};

		void Nested()
		{}
	}

	inline namespace Version1
	{
		struct Versioned
		{};
	}

	namespace InnerAlias = Inner;

	namespace
	{
		struct FileLocal
		{};
	}
}

namespace TemplatesOnly
{
	template <typename T>
	struct OnlyATemplate
	{};

	template <typename T>
	void OnlyAFunctionTemplate(T)
	{}
}

// Reopened below its first half: the sweep must see the union of both, since a namespace is not closed the
// way a class is.
namespace SweepFixture
{
	struct Reopened
	{};

	void ReopenedFunction()
	{}
}

namespace
{
	std::vector<std::string_view> IdentifiersOf(const std::span<const PgE::NestedTypeInfo> types)
	{
		std::vector<std::string_view> identifiers;
		for (const PgE::NestedTypeInfo& type : types)
		{
			identifiers.push_back(type.GetIdentifier());
		}
		return identifiers;
	}

	template <typename T>
	std::vector<std::string_view> IdentifiersOf(const std::span<const T* const> entities)
	{
		std::vector<std::string_view> identifiers;
		for (const T* entity : entities)
		{
			identifiers.push_back(entity->GetIdentifier());
		}
		return identifiers;
	}

	bool Contains(const std::vector<std::string_view>& identifiers, const std::string_view identifier)
	{
		return std::ranges::contains(identifiers, identifier);
	}
}

TEST_CASE("a namespace reports the entities declared directly in it, across all its reopenings")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	CHECK(fixture.GetIdentifier() == "SweepFixture");

	const auto types = IdentifiersOf(fixture.GetTypes());
	CHECK(Contains(types, "Widget"));
	CHECK(Contains(types, "Color"));
	CHECK(Contains(types, "Reopened"));

	const auto functions = IdentifiersOf(fixture.GetFunctions());
	CHECK(Contains(functions, "Unique"));
	CHECK(Contains(functions, "ReopenedFunction"));

	const auto variables = IdentifiersOf(fixture.GetVariables());
	CHECK(Contains(variables, "Counter"));
	CHECK(Contains(variables, "MaxSlots"));

	const auto namespaces = IdentifiersOf(fixture.GetNamespaces());
	CHECK(Contains(namespaces, "Inner"));
}

TEST_CASE("a swept entity is the same object as the one reached by naming it")
{
	// The property the whole system rests on: metadata is compared by pointer identity, so discovering an
	// entity by sweeping and naming it directly must not produce two descriptions of it.
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	const auto unique =
		std::ranges::find_if(fixture.GetFunctions(), [](const PgE::FunctionInfo* function) { return function->GetIdentifier() == "Unique"; });
	REQUIRE(unique != fixture.GetFunctions().end());
	CHECK(*unique == &PgE::detail::FunctionOfMeta<^^SweepFixture::Unique>());

	const auto counter =
		std::ranges::find_if(fixture.GetVariables(), [](const PgE::StaticFieldInfo* variable) { return variable->GetIdentifier() == "Counter"; });
	REQUIRE(counter != fixture.GetVariables().end());
	CHECK(*counter == &PgE::detail::VariableOfMeta<^^SweepFixture::Counter>());

	const auto widget = std::ranges::find_if(fixture.GetTypes(), [](const PgE::NestedTypeInfo& type) { return type.GetIdentifier() == "Widget"; });
	REQUIRE(widget != fixture.GetTypes().end());
	CHECK(&widget->GetTypeInfo() == &PgE::TypeOf<SweepFixture::Widget>());

	CHECK(&PgE::NamespaceOf<^^SweepFixture>() == &fixture);
}

TEST_CASE("a swept entity carries its full metadata, not just a name")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	const auto unique =
		std::ranges::find_if(fixture.GetFunctions(), [](const PgE::FunctionInfo* function) { return function->GetIdentifier() == "Unique"; });
	REQUIRE(unique != fixture.GetFunctions().end());
	CHECK((*unique)->IsFreeFunction());
	CHECK((*unique)->GetAccess() == PgE::AccessKind::None);

	const auto invoked = (*unique)->InvokeAs<int>(static_cast<void*>(nullptr), 21);
	REQUIRE(invoked.has_value());
	CHECK(*invoked == 42);

	const auto maxSlots =
		std::ranges::find_if(fixture.GetVariables(), [](const PgE::StaticFieldInfo* variable) { return variable->GetIdentifier() == "MaxSlots"; });
	REQUIRE(maxSlots != fixture.GetVariables().end());
	CHECK((*maxSlots)->IsConstantReadable());
	CHECK((*maxSlots)->GetAs<int>().value_or(-1) == 8);
}

TEST_CASE("overloads are all reported, which is the only way to reach them")
{
	// ^^SweepFixture::Overloaded is ill-formed ("cannot take the reflection of an overload set"), so naming
	// an overloaded free function is impossible and the sweep is its sole route into reflection.
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	std::vector<const PgE::FunctionInfo*> overloads;
	for (const PgE::FunctionInfo* function : fixture.GetFunctions())
	{
		if (function->GetIdentifier() == "Overloaded")
		{
			overloads.push_back(function);
		}
	}

	REQUIRE(overloads.size() == 2);
	CHECK(overloads[0] != overloads[1]);
	CHECK(overloads[0]->GetParams().size() != overloads[1]->GetParams().size());
}

TEST_CASE("a reference variable is omitted, so one declaration cannot fail a whole namespace")
{
	// The rejection has to live in the sweep: the crash happens in the compiler's symbol table, too late for
	// a static_assert in VariableOfMeta to intercept, so naming one directly is still unguarded.
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetVariables()), "ReferenceVariable"));
	CHECK(Contains(IdentifiersOf(fixture.GetVariables()), "Counter"));
}

TEST_CASE("thread storage duration is reported, so per-thread state is not mistaken for global state")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	const auto perThread =
		std::ranges::find_if(fixture.GetVariables(), [](const PgE::StaticFieldInfo* variable) { return variable->GetIdentifier() == "PerThread"; });
	REQUIRE(perThread != fixture.GetVariables().end());
	CHECK((*perThread)->IsThreadLocal());
	CHECK((*perThread)->GetAs<int>().value_or(-1) == 5);

	const auto counter =
		std::ranges::find_if(fixture.GetVariables(), [](const PgE::StaticFieldInfo* variable) { return variable->GetIdentifier() == "Counter"; });
	REQUIRE(counter != fixture.GetVariables().end());
	CHECK_FALSE((*counter)->IsThreadLocal());
}

TEST_CASE("a type alias is reported under its own name and resolves to what it aliases")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	const auto alias =
		std::ranges::find_if(fixture.GetTypes(), [](const PgE::NestedTypeInfo& type) { return type.GetIdentifier() == "WidgetAlias"; });
	REQUIRE(alias != fixture.GetTypes().end());
	CHECK(alias->IsAlias());
	CHECK(&alias->GetTypeInfo() == &PgE::TypeOf<SweepFixture::Widget>());
}

TEST_CASE("templates and concepts are not swept, because neither is reflectable until instantiated")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetTypes()), "Grid"));
	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetFunctions()), "Generic"));
	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetTypes()), "Constrained"));
}

TEST_CASE("a namespace alias resolves to the namespace it names, reported once")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	CHECK(&PgE::NamespaceOf<^^SweepFixture::InnerAlias>() == &PgE::NamespaceOf<^^SweepFixture::Inner>());

	const auto namespaces = IdentifiersOf(fixture.GetNamespaces());
	CHECK(Contains(namespaces, "Inner"));
	CHECK_FALSE(Contains(namespaces, "InnerAlias"));
}

TEST_CASE("an anonymous namespace is not swept, having no name to key on and no identity outside this TU")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	for (const PgE::NamespaceInfo* nested : fixture.GetNamespaces())
	{
		CHECK_FALSE(nested->GetIdentifier().empty());
	}

	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetTypes()), "FileLocal"));
}

TEST_CASE("an inline namespace is an ordinary nested namespace, and its members are not lifted into the parent")
{
	// Characterizes a language-visible gap: name lookup finds SweepFixture::Versioned, but members_of does
	// not, and std::meta cannot tell an inline namespace from a plain one. A consumer must recurse.
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	CHECK_FALSE(Contains(IdentifiersOf(fixture.GetTypes()), "Versioned"));

	const auto version1 =
		std::ranges::find_if(fixture.GetNamespaces(), [](const PgE::NamespaceInfo* nested) { return nested->GetIdentifier() == "Version1"; });
	REQUIRE(version1 != fixture.GetNamespaces().end());
	CHECK(Contains(IdentifiersOf((*version1)->GetTypes()), "Versioned"));

	// The type's own scope path does cross the inline namespace, so the two disagree by design.
	CHECK(&PgE::TypeOf<SweepFixture::Versioned>() == &PgE::TypeOf<SweepFixture::Version1::Versioned>());
	CHECK(PgE::TypeOf<SweepFixture::Versioned>().GetScopePath().back() == "Version1");
}

TEST_CASE("a nested namespace is reflected recursively and reports its scope")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	const auto inner =
		std::ranges::find_if(fixture.GetNamespaces(), [](const PgE::NamespaceInfo* nested) { return nested->GetIdentifier() == "Inner"; });
	REQUIRE(inner != fixture.GetNamespaces().end());

	CHECK(Contains(IdentifiersOf((*inner)->GetTypes()), "Deep"));
	CHECK(Contains(IdentifiersOf((*inner)->GetFunctions()), "Nested"));

	REQUIRE((*inner)->GetScopePath().size() == 1);
	CHECK((*inner)->GetScopePath().front() == "SweepFixture");
}

TEST_CASE("a namespace is an annotatable declaration, which is what a consumer filters a sweep on")
{
	const PgE::NamespaceInfo& fixture = PgE::NamespaceOf<^^SweepFixture>();

	REQUIRE(fixture.HasAnnotation<Marker>());

	const auto markers = fixture.GetAnnotations<Marker>();
	REQUIRE(markers.size() == 1);
	CHECK(markers.front()->Id == 7);

	CHECK_FALSE(PgE::NamespaceOf<^^SweepFixture::Inner>().HasAnnotation<Marker>());
}

TEST_CASE("a namespace holding only unreflectable members sweeps to an empty set, not a failure")
{
	const PgE::NamespaceInfo& templatesOnly = PgE::NamespaceOf<^^TemplatesOnly>();

	CHECK(templatesOnly.GetTypes().empty());
	CHECK(templatesOnly.GetFunctions().empty());
	CHECK(templatesOnly.GetVariables().empty());
	CHECK(templatesOnly.GetNamespaces().empty());
}
