#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Reflection;
import PlaygroundTests.ReflectionTestTypes;

using namespace ReflectionTestTypes;

namespace
{
	// Internal linkage: reflection must report it, since such a type has no identity outside this TU.
	struct FileLocal
	{
		int Value = 0;
	};

	// Forward-declared and never defined: the opaque-handle / PIMPL shape. Reachable only because a pointer
	// now names its pointee, and every member walk throws on an incomplete type.
	struct Opaque;

	struct HasOpaquePointer
	{
		Opaque* Handle = nullptr;
		int Plain = 3;
	};

	const PgE::FieldInfo& FieldNamed(const PgE::TypeInfo& type, const std::string_view identifier)
	{
		const PgE::FieldInfo* field = type.FindFieldByIdentifier(identifier);
		REQUIRE(field != nullptr);
		return *field;
	}

	const PgE::FunctionInfo& FunctionNamed(const PgE::TypeInfo& type, const std::string_view identifier)
	{
		const auto functions = type.FindFunctionsByIdentifier(identifier);
		REQUIRE(functions.size() == 1);
		return *functions.front();
	}

	// A consumer-side renderer built strictly from extracted metadata: no display string is ever read. It
	// exists to prove the extraction is sufficient to name any type, which is the whole point of the
	// structural naming, the scope path, the template arguments, and the compound decomposition together.
	std::string RenderTypeName(const PgE::TypeInfo& type)
	{
		if (type.GetTraits().IsConst)
		{
			return std::format("const<{}>", RenderTypeName(type.GetInnerType()));
		}

		switch (type.GetKind())
		{
		case PgE::TypeKind::Pointer:
			return std::format("pointer<{}>", RenderTypeName(type.GetInnerType()));
		case PgE::TypeKind::LValueReference:
			return std::format("ref<{}>", RenderTypeName(type.GetInnerType()));
		case PgE::TypeKind::Array:
			return std::format("array<{}, {}>", RenderTypeName(type.GetInnerType()), type.GetTraits().Extent);
		default:
			break;
		}

		std::string name;
		for (const std::string_view scope : type.GetScopePath())
		{
			name += scope;
			name += ".";
		}

		if (type.GetTemplate() == nullptr)
		{
			return name + std::string(type.GetIdentifier());
		}

		// A template instance has no identifier of its own: it is named by its template plus its arguments.
		name = {};
		for (const std::string_view scope : type.GetTemplate()->GetScopePath())
		{
			name += scope;
			name += ".";
		}
		name += type.GetTemplate()->GetIdentifier();

		name += "<";
		bool first = true;
		for (const PgE::TemplateArgumentInfo& argument : type.GetTemplateArguments())
		{
			if (!std::exchange(first, false))
			{
				name += ", ";
			}

			switch (argument.Kind)
			{
			case PgE::TemplateArgumentKind::Type:
				name += RenderTypeName(argument.Type.Get());
				break;
			case PgE::TemplateArgumentKind::Value:
				name += std::format("{}", *static_cast<const int*>(argument.Value));
				break;
			case PgE::TemplateArgumentKind::Template:
				name += argument.Template->GetIdentifier();
				break;
			}
		}
		name += ">";

		return name;
	}
}

TEST_CASE("scope path reports the enclosing named scopes, outermost first")
{
	const auto scope = PgE::TypeOf<Widget>().GetScopePath();
	REQUIRE(scope.size() == 1);
	CHECK(scope[0] == "ReflectionTestTypes");

	const auto nested = PgE::TypeOf<Deep::Buried>().GetScopePath();
	REQUIRE(nested.size() == 2);
	CHECK(nested[0] == "ReflectionTestTypes");
	CHECK(nested[1] == "Deep");
}

TEST_CASE("scope path distinguishes a member from its owning type, and is empty at global scope")
{
	// The walk crosses inline namespaces, which is the correct structural answer and the reason a durable
	// identifier must not be built from a std type's path.
	const auto stringScope = PgE::TypeOf<std::string>().GetScopePath();
	REQUIRE(stringScope.size() >= 1);
	CHECK(stringScope[0] == "std");

	// A fundamental type has no parent at all: the walk must terminate rather than throw.
	CHECK(PgE::TypeOf<int>().GetScopePath().empty());
	CHECK(PgE::TypeOf<int*>().GetScopePath().empty());

	// A field's scope is its owning type, so A::Foo and B::Foo stay distinguishable.
	const PgE::FieldInfo& health = FieldNamed(PgE::TypeOf<Gadget>(), "Health");
	const auto fieldScope = health.GetScopePath();
	REQUIRE(fieldScope.size() == 2);
	CHECK(fieldScope[0] == "ReflectionTestTypes");
	CHECK(fieldScope[1] == "Gadget");
}

TEST_CASE("a fundamental type is named from structure, not spelling")
{
	CHECK(PgE::TypeOf<std::int32_t>().GetIdentifier() == "int32");
	CHECK(PgE::TypeOf<std::uint64_t>().GetIdentifier() == "uint64");
	CHECK(PgE::TypeOf<float>().GetIdentifier() == "float32");
	CHECK(PgE::TypeOf<double>().GetIdentifier() == "float64");
	CHECK(PgE::TypeOf<bool>().GetIdentifier() == "bool");
	CHECK(PgE::TypeOf<void>().GetIdentifier() == "void");

	// The width is the fact, so every alias of the same type reports one name.
	CHECK(PgE::TypeOf<unsigned long long>().GetIdentifier() == PgE::TypeOf<std::uint64_t>().GetIdentifier());

	// char is not derived from size and signedness: that would collapse it with signed char and int8_t,
	// which are distinct types, and char is the string element type.
	CHECK(PgE::TypeOf<char>().GetIdentifier() == "char");
	CHECK(PgE::TypeOf<signed char>().GetIdentifier() == "int8");
	CHECK(PgE::TypeOf<char8_t>().GetIdentifier() == "char8_t");
	CHECK(&PgE::TypeOf<char>() != &PgE::TypeOf<signed char>());
}

TEST_CASE("linkage discriminates external, module, and internal types")
{
	CHECK(PgE::TypeOf<Widget>().GetTraits().Linkage == PgE::LinkageKind::External);
	CHECK(PgE::TypeOf<ModuleLocalAlias>().GetTraits().Linkage == PgE::LinkageKind::Module);
	CHECK(PgE::TypeOf<FileLocal>().GetTraits().Linkage == PgE::LinkageKind::Internal);
}

TEST_CASE("is_final is reported")
{
	CHECK(PgE::TypeOf<FinalType>().GetTraits().IsFinal);
	CHECK_FALSE(PgE::TypeOf<Widget>().GetTraits().IsFinal);
}

TEST_CASE("a template instance names its primary template and its arguments")
{
	const PgE::TypeInfo& grid = PgE::TypeOf<Grid<int>>();
	REQUIRE(grid.GetTemplate() != nullptr);
	CHECK(grid.GetTemplate()->GetIdentifier() == "Grid");
	CHECK(grid.GetIdentifier().empty());

	const auto arguments = grid.GetTemplateArguments();
	REQUIRE(arguments.size() == 1);
	CHECK(arguments[0].Kind == PgE::TemplateArgumentKind::Type);
	CHECK(&arguments[0].Type.Get() == &PgE::TypeOf<int>());

	// One TemplateInfo per template, so two instances of the same template name the same one.
	CHECK(PgE::TypeOf<Grid<int>>().GetTemplate() == PgE::TypeOf<Grid<float>>().GetTemplate());

	// A non-instance has no template.
	CHECK(PgE::TypeOf<Widget>().GetTemplate() == nullptr);
	CHECK(PgE::TypeOf<Widget>().GetTemplateArguments().empty());
}

TEST_CASE("a template is a named entity, not an annotatable declaration")
{
	// The language decides this, not us: annotations_of accepts a type, alias, variable, function, parameter,
	// namespace, enumerator, base, or data member, and a template is none of them. So TemplateInfo is an
	// EntityInfo, and never inherits an annotation API that could only ever answer "empty".
	static_assert(std::derived_from<PgE::TemplateInfo, PgE::EntityInfo>);
	static_assert(!std::derived_from<PgE::TemplateInfo, PgE::DeclarationInfo>);

	// Every other Info is annotatable, so it stays a DeclarationInfo.
	static_assert(std::derived_from<PgE::TypeInfo, PgE::DeclarationInfo>);
	static_assert(std::derived_from<PgE::FieldInfo, PgE::DeclarationInfo>);
	static_assert(std::derived_from<PgE::DeclarationInfo, PgE::EntityInfo>);

	// Naming still works through the base, which is the whole of what a template has to offer.
	const PgE::TemplateInfo* grid = PgE::TypeOf<Grid<int>>().GetTemplate();
	REQUIRE(grid != nullptr);
	CHECK(grid->GetIdentifier() == "Grid");
	REQUIRE(grid->GetScopePath().size() == 1);
	CHECK(grid->GetScopePath()[0] == "ReflectionTestTypes");
}

TEST_CASE("an annotation on a class template is reached through the instance")
{
	// Nothing is lost by a template not carrying annotations: the language lands them on each instance, which
	// is a type, and therefore already a DeclarationInfo.
	const PgE::TypeInfo& instance = PgE::TypeOf<AnnotatedGrid<int>>();
	REQUIRE(instance.HasAnnotation<Doc>());
	CHECK(std::string_view(instance.GetAnnotations<Doc>().front()->Text) == "a annotated grid");

	// It is the instance that is annotated, so a different instance carries it too.
	CHECK(PgE::TypeOf<AnnotatedGrid<char>>().HasAnnotation<Doc>());
}

TEST_CASE("a non-type template argument carries its value")
{
	const auto arguments = PgE::TypeOf<FixedArray<int, 8>>().GetTemplateArguments();
	REQUIRE(arguments.size() == 2);

	CHECK(arguments[0].Kind == PgE::TemplateArgumentKind::Type);
	CHECK(&arguments[0].Type.Get() == &PgE::TypeOf<int>());

	CHECK(arguments[1].Kind == PgE::TemplateArgumentKind::Value);
	CHECK(&arguments[1].Type.Get() == &PgE::TypeOf<int>());
	REQUIRE(arguments[1].Value != nullptr);
	CHECK(*static_cast<const int*>(arguments[1].Value) == 8);
}

TEST_CASE("a non-type argument that names an object erases to that object, not a copy")
{
	// A reference parameter reflects as an object, not a value: reading it as one is not a constant
	// expression, so it takes the address instead of materializing a copy. The two forms are easy to
	// conflate, and conflating them fails to compile rather than fail quietly.
	const auto refArgs = PgE::TypeOf<BoundToRef<SharedSlot>>().GetTemplateArguments();
	REQUIRE(refArgs.size() == 1);
	CHECK(refArgs[0].Kind == PgE::TemplateArgumentKind::Value);
	CHECK(&refArgs[0].Type.Get() == &PgE::TypeOf<int>());
	CHECK(refArgs[0].Value == &SharedSlot);

	// A pointer parameter's value really is the pointer, so it is materialized like any other value.
	const auto ptrArgs = PgE::TypeOf<BoundToPtr<&SharedSlot>>().GetTemplateArguments();
	REQUIRE(ptrArgs.size() == 1);
	CHECK(ptrArgs[0].Kind == PgE::TemplateArgumentKind::Value);
	CHECK(&ptrArgs[0].Type.Get() == &PgE::TypeOf<int*>());
	CHECK(*static_cast<const int* const*>(ptrArgs[0].Value) == &SharedSlot);

	// An enum argument keeps its own type, not its underlying one.
	const auto enumArgs = PgE::TypeOf<Configured<Mode::On>>().GetTemplateArguments();
	REQUIRE(enumArgs.size() == 1);
	CHECK(&enumArgs[0].Type.Get() == &PgE::TypeOf<Mode>());
	CHECK(*static_cast<const Mode*>(enumArgs[0].Value) == Mode::On);
}

TEST_CASE("a template template argument is a third kind, neither type nor value")
{
	const auto arguments = PgE::TypeOf<Holder<Grid, Inner>>().GetTemplateArguments();
	REQUIRE(arguments.size() == 2);

	CHECK(arguments[0].Kind == PgE::TemplateArgumentKind::Template);
	REQUIRE(arguments[0].Template != nullptr);
	CHECK(arguments[0].Template->GetIdentifier() == "Grid");

	CHECK(arguments[1].Kind == PgE::TemplateArgumentKind::Type);
	CHECK(&arguments[1].Type.Get() == &PgE::TypeOf<Inner>());
}

TEST_CASE("a partial specialization reports the primary template, and a defaulted argument is materialized")
{
	REQUIRE(PgE::TypeOf<Spec<int*>>().GetTemplate() != nullptr);
	CHECK(PgE::TypeOf<Spec<int*>>().GetTemplate() == PgE::TypeOf<Spec<int>>().GetTemplate());

	// Stream<Inner> and Stream<Inner, DefaultPolicy> are the same type and must reflect identically, so the
	// defaulted argument is present and indistinguishable from a written one.
	const auto arguments = PgE::TypeOf<Stream<Inner>>().GetTemplateArguments();
	REQUIRE(arguments.size() == 2);
	CHECK(&arguments[1].Type.Get() == &PgE::TypeOf<DefaultPolicy>());
	CHECK(&PgE::TypeOf<Stream<Inner>>() == &PgE::TypeOf<Stream<Inner, DefaultPolicy>>());
}

TEST_CASE("an alias template dealiases away entirely")
{
	CHECK(&PgE::TypeOf<PtrAlias<Inner>>() == &PgE::TypeOf<Inner*>());
	CHECK(PgE::TypeOf<PtrAlias<Inner>>().GetTemplate() == nullptr);
}

TEST_CASE("a compound type decomposes to the type it is built from")
{
	const PgE::TypeInfo& pointer = PgE::TypeOf<Inner*>();
	CHECK(pointer.GetKind() == PgE::TypeKind::Pointer);
	REQUIRE(pointer.HasInnerType());
	CHECK(&pointer.GetInnerType() == &PgE::TypeOf<Inner>());

	// Each level peels exactly one shape, so a walk chains down to a name.
	const PgE::TypeInfo& pointerToPointer = PgE::TypeOf<Inner**>();
	CHECK(&pointerToPointer.GetInnerType() == &PgE::TypeOf<Inner*>());
	CHECK(&pointerToPointer.GetInnerType().GetInnerType() == &PgE::TypeOf<Inner>());

	const PgE::TypeInfo& array = PgE::TypeOf<Inner[4]>();
	CHECK(array.GetKind() == PgE::TypeKind::Array);
	CHECK(array.GetTraits().Extent == 4);
	CHECK(&array.GetInnerType() == &PgE::TypeOf<Inner>());

	const PgE::TypeInfo& reference = PgE::TypeOf<Inner&>();
	CHECK(reference.GetKind() == PgE::TypeKind::LValueReference);
	CHECK(&reference.GetInnerType() == &PgE::TypeOf<Inner>());

	// A type that decomposes no further is where a walk bottoms out on a name.
	CHECK_FALSE(PgE::TypeOf<Inner>().HasInnerType());
	CHECK_FALSE(PgE::TypeOf<int>().HasInnerType());
}

TEST_CASE("a cv-qualified type is a decomposition node, not a second copy of the class")
{
	const PgE::TypeInfo& constInner = PgE::TypeOf<const Inner>();
	CHECK(constInner.GetTraits().IsConst);
	CHECK(constInner.GetIdentifier().empty());
	REQUIRE(constInner.HasInnerType());
	CHECK(&constInner.GetInnerType() == &PgE::TypeOf<Inner>());

	// The structure lives on the unqualified type only: walking const Inner would otherwise duplicate
	// Inner's whole field list under a second identity (is_class_type(^^const Inner) is true).
	CHECK(constInner.GetFields().empty());
	CHECK_FALSE(PgE::TypeOf<Inner>().GetFields().empty());

	// const Inner* reaches Inner through the const node rather than skipping it.
	const PgE::TypeInfo& pointerToConst = PgE::TypeOf<const Inner*>();
	CHECK(&pointerToConst.GetInnerType() == &constInner);
	CHECK(&pointerToConst.GetInnerType().GetInnerType() == &PgE::TypeOf<Inner>());
	CHECK(&PgE::TypeOf<Inner*>() != &pointerToConst);
}

TEST_CASE("a field reports its bit width, access, default initializer, and mutability")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Members>();

	const PgE::FieldInfo& small = FieldNamed(type, "Small");
	CHECK(small.IsBitField());
	CHECK(small.GetBitSize() == 3);
	CHECK(FieldNamed(type, "Wide").GetBitSize() == 10);

	// A non-bitfield has no width to report.
	const PgE::FieldInfo& plain = FieldNamed(type, "Plain");
	CHECK_FALSE(plain.IsBitField());
	CHECK(plain.GetBitSize() == 0);

	CHECK(FieldNamed(type, "WithDefault").HasDefaultInitializer());
	CHECK_FALSE(plain.HasDefaultInitializer());

	CHECK(FieldNamed(type, "Cached").IsMutable());
	CHECK_FALSE(plain.IsMutable());

	CHECK(plain.GetAccess() == PgE::AccessKind::Public);
	CHECK(FieldNamed(type, "Guarded").GetAccess() == PgE::AccessKind::Protected);
	CHECK(FieldNamed(type, "Hidden").GetAccess() == PgE::AccessKind::Private);
}

TEST_CASE("a function reports static and const as separate facts")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Surface>();

	const PgE::FunctionInfo& stat = FunctionNamed(type, "Stat");
	CHECK(stat.IsStatic());
	CHECK_FALSE(stat.IsConst());

	const PgE::FunctionInfo& constant = FunctionNamed(type, "Constant");
	CHECK(constant.IsConst());
	CHECK_FALSE(constant.IsStatic());

	// The invoke path still wants "callable on a const object", which is now derived from the two facts
	// rather than being the single stored bool that conflated them.
	CHECK(stat.IsConstCallable());
	CHECK(constant.IsConstCallable());
	CHECK_FALSE(FunctionNamed(type, "Plain").IsConstCallable());
}

TEST_CASE("a function reports its remaining language facts")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Surface>();

	CHECK(FunctionNamed(type, "Never").IsNoexcept());
	CHECK_FALSE(FunctionNamed(type, "Plain").IsNoexcept());

	CHECK(FunctionNamed(type, "Virt").IsVirtual());
	CHECK(FunctionNamed(type, "Pure").IsPureVirtual());
	CHECK_FALSE(FunctionNamed(type, "Virt").IsPureVirtual());
	CHECK(FunctionNamed(PgE::TypeOf<SurfaceChild>(), "Virt").IsOverride());

	CHECK(FunctionNamed(type, "Gone").IsDeleted());
	CHECK_FALSE(FunctionNamed(type, "Plain").IsDeleted());

	CHECK(FunctionNamed(type, "Plain").GetAccess() == PgE::AccessKind::Public);
	CHECK(FunctionNamed(type, "Guarded").GetAccess() == PgE::AccessKind::Protected);
}

TEST_CASE("a reference qualifier turns a missing invoker into a stated reason")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Surface>();

	CHECK(FunctionNamed(type, "OnLvalue").GetRefQualifier() == PgE::RefQualifier::LValue);
	CHECK(FunctionNamed(type, "Plain").GetRefQualifier() == PgE::RefQualifier::None);

	// An rvalue-ref-qualified function reflects with no invoker; before, the metadata gave no reason why.
	const PgE::FunctionInfo& onRvalue = FunctionNamed(type, "OnRvalue");
	CHECK(onRvalue.GetRefQualifier() == PgE::RefQualifier::RValue);
	CHECK(onRvalue.Invoke(static_cast<void*>(nullptr), {}).error().Reason == PgE::InvokeError::NotInvocable);
}

TEST_CASE("a decayed return type keeps its qualifiers as stated facts")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Surface>();

	// The stored return type is decayed, so these are what say InvokeAs<const std::string&> may work.
	const PgE::FunctionInfo& byConstRef = FunctionNamed(type, "ByConstRef");
	CHECK(&byConstRef.GetReturnType() == &PgE::TypeOf<std::string>());
	CHECK(byConstRef.GetTraits().ReturnIsLvalueReference);
	CHECK(byConstRef.GetTraits().ReturnIsConst);

	const PgE::FunctionInfo& plain = FunctionNamed(type, "Plain");
	CHECK_FALSE(plain.GetTraits().ReturnIsLvalueReference);
	CHECK_FALSE(plain.GetTraits().ReturnIsConst);
}

TEST_CASE("a parameter reports its default argument and its qualifiers")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Surface>();

	const auto defaulted = FunctionNamed(type, "Defaulted").GetParams();
	REQUIRE(defaulted.size() == 2);
	CHECK_FALSE(defaulted[0].HasDefaultArgument());
	CHECK(defaulted[1].HasDefaultArgument());

	// The sink pair: both parameters store the same decayed tag, and only the qualifiers separate them.
	const auto qualified = FunctionNamed(type, "Qualified").GetParams();
	REQUIRE(qualified.size() == 2);
	CHECK(&qualified[0].GetTypeInfo() == &qualified[1].GetTypeInfo());

	CHECK(qualified[0].IsConst());
	CHECK(qualified[0].IsLvalueReference());
	CHECK_FALSE(qualified[0].IsRvalueReference());

	CHECK_FALSE(qualified[1].IsConst());
	CHECK(qualified[1].IsRvalueReference());
	CHECK_FALSE(qualified[1].IsLvalueReference());
}

TEST_CASE("static data members are reflected separately from fields")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Registry>();

	// They are not entries in the field list: they have no offset and no instance.
	CHECK(type.FindFieldByIdentifier("Counter") == nullptr);
	REQUIRE(type.GetStaticFields().size() == 4);
	CHECK(type.FindFieldByIdentifier("Instance") != nullptr);

	const PgE::StaticFieldInfo* counter = type.FindStaticFieldByIdentifier("Counter");
	REQUIRE(counter != nullptr);
	CHECK(&counter->GetTypeInfo() == &PgE::TypeOf<int>());
	CHECK(counter->GetAccess() == PgE::AccessKind::Public);
}

TEST_CASE("a constant-readable static is captured by value and is read-only")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Registry>();

	// MaxSlots has no out-of-line definition, so its address cannot be taken: forming it is well-formed and
	// fails only at link, which no requires can detect. Capturing the value dodges the odr-use entirely.
	const PgE::StaticFieldInfo* maxSlots = type.FindStaticFieldByIdentifier("MaxSlots");
	REQUIRE(maxSlots != nullptr);
	CHECK(maxSlots->IsConstantReadable());

	const auto value = maxSlots->GetAs<int>();
	REQUIRE(value.has_value());
	CHECK(*value == 8);

	// Read-only is the semantics of a value, not a workaround: there is no object to write or borrow.
	CHECK(maxSlots->SetAs<int>(9).error().Reason == PgE::FieldError::NotWritable);
	CHECK(maxSlots->GetRef().error().Reason == PgE::FieldError::NotAddressable);

	const PgE::StaticFieldInfo* scale = type.FindStaticFieldByIdentifier("Scale");
	REQUIRE(scale != nullptr);
	CHECK(scale->IsConstantReadable());
	CHECK(*scale->GetAs<double>() == doctest::Approx(2.5));
}

TEST_CASE("an addressable static is settable through reflection")
{
	const PgE::TypeInfo& type = PgE::TypeOf<Registry>();

	const PgE::StaticFieldInfo* counter = type.FindStaticFieldByIdentifier("Counter");
	REQUIRE(counter != nullptr);
	CHECK_FALSE(counter->IsConstantReadable());

	Registry::Counter = 0;
	REQUIRE(counter->SetAs<int>(11).has_value());
	CHECK(Registry::Counter == 11);
	CHECK(*counter->GetAs<int>() == 11);

	const auto ref = counter->GetRef();
	REQUIRE(ref.has_value());
	CHECK(ref->Data == &Registry::Counter);

	Registry::Counter = 0;

	// A non-constant-readable const member takes the address path, so it is readable but not settable.
	const PgE::StaticFieldInfo* label = type.FindStaticFieldByIdentifier("Label");
	REQUIRE(label != nullptr);
	CHECK_FALSE(label->IsConstantReadable());
	CHECK(*label->GetAs<std::string>() == "registry");
	CHECK(label->SetAs<std::string>("other").error().Reason == PgE::FieldError::NotWritable);
}

TEST_CASE("a field's type is fully identifiable with no spelling anywhere")
{
	// The claim the extraction exists to support: every name below is composed from stored facts alone
	// (scope path, structural fundamental names, template plus arguments, compound decomposition). Not one
	// display string is read, so a non-native provider could describe the same types with no C++ spelling.
	CHECK(RenderTypeName(PgE::TypeOf<Inner**>()) == "pointer<pointer<ReflectionTestTypes.Inner>>");
	CHECK(RenderTypeName(PgE::TypeOf<const Inner*>()) == "pointer<const<ReflectionTestTypes.Inner>>");
	CHECK(RenderTypeName(PgE::TypeOf<Inner[4]>()) == "array<ReflectionTestTypes.Inner, 4>");
	CHECK(RenderTypeName(PgE::TypeOf<Grid<Inner*>>()) == "ReflectionTestTypes.Grid<pointer<ReflectionTestTypes.Inner>>");
	CHECK(RenderTypeName(PgE::TypeOf<FixedArray<Inner, 8>>()) == "ReflectionTestTypes.FixedArray<ReflectionTestTypes.Inner, 8>");
	CHECK(RenderTypeName(PgE::TypeOf<unsigned long long>()) == "uint64");
	CHECK(RenderTypeName(PgE::TypeOf<Deep::Buried>()) == "ReflectionTestTypes.Deep.Buried");

	// Reached from a real field, which is the case that matters: a consumer walking fields never hits a hole.
	const PgE::FieldInfo& pointer = FieldNamed(PgE::TypeOf<Outer>(), "Pointer");
	CHECK(RenderTypeName(pointer.GetTypeInfo()) == "pointer<int32>");
	CHECK(RenderTypeName(FieldNamed(PgE::TypeOf<Outer>(), "Plain").GetTypeInfo()) == "int32");
}

TEST_CASE("a pointer to an incomplete type is named but has no structure")
{
	// An opaque handle behind a pointer is a shape the engine has to support, and decomposing the pointer is
	// what reaches the incomplete type at all: every member walk and every layout query throws on one.
	const PgE::TypeInfo& type = PgE::TypeOf<HasOpaquePointer>();
	REQUIRE(type.GetFields().size() == 2);

	const PgE::TypeInfo& pointer = FieldNamed(type, "Handle").GetTypeInfo();
	REQUIRE(pointer.HasInnerType());

	// Naming it is all that is possible: its definition is what would decide the rest, so those stay unset
	// rather than being guessed.
	const PgE::TypeInfo& opaque = pointer.GetInnerType();
	CHECK(opaque.GetIdentifier() == "Opaque");
	CHECK(opaque.GetFields().empty());
	CHECK(opaque.GetSize() == 0);
	CHECK(opaque.GetAlignment() == 0);
	CHECK_FALSE(opaque.GetTraits().IsTriviallyCopyable);
	CHECK_FALSE(opaque.CanDestroy());
	CHECK_FALSE(opaque.CanStringify());

	// The owning type is unaffected: its real fields still read.
	HasOpaquePointer value{};
	CHECK(*type.GetFieldAs<int>(&value, "Plain") == 3);
}

TEST_CASE("a std container renders through the same extraction, allocator included")
{
	// The allocator is exposed as a materialized template argument, which is the correct structural answer.
	// It is also the type whose always_inline members have no address, so this is the case that forced the
	// invoke path to take one only where access control demands it.
	CHECK(RenderTypeName(PgE::TypeOf<std::vector<Inner>>()) == "std.vector<ReflectionTestTypes.Inner, std.allocator<ReflectionTestTypes.Inner>>");
}

TEST_CASE("a private member function stays invocable, which is why the pointer route exists")
{
	// An address is taken only where access control demands it, so this is the case that still needs one.
	const PgE::TypeInfo& type = PgE::TypeOf<WithPrivate>();
	WithPrivate object{.Value = 21};

	const PgE::FunctionInfo& doubled = FunctionNamed(type, "Doubled");
	CHECK(doubled.GetAccess() == PgE::AccessKind::Private);

	const auto result = doubled.InvokeAs<int>(&object);
	REQUIRE(result.has_value());
	CHECK(*result == 42);
}
