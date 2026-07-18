#include <doctest/doctest.h>
#include <meta>

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

TEST_CASE("a function type carries its call shape, so a callback is not a dead end")
{
	// A function type has no inner type: it decomposes into a return type and a parameter list instead.
	const PgE::TypeInfo& function = PgE::TypeOf<void(int, float)>();
	CHECK(function.GetKind() == PgE::TypeKind::Function);
	CHECK_FALSE(function.HasInnerType());

	const PgE::FunctionSignatureInfo* signature = function.GetSignature();
	REQUIRE(signature != nullptr);
	CHECK(&signature->GetReturnType() == &PgE::TypeOf<void>());
	REQUIRE(signature->GetParameters().size() == 2);
	CHECK(&signature->GetParameters()[0].Get() == &PgE::TypeOf<int>());
	CHECK(&signature->GetParameters()[1].Get() == &PgE::TypeOf<float>());
	CHECK_FALSE(signature->IsNoexcept());
	CHECK_FALSE(signature->IsVariadic());

	// noexcept is part of the type, so it reaches the signature rather than being erased.
	const PgE::FunctionSignatureInfo* noexceptSignature = PgE::TypeOf<int(double) noexcept>().GetSignature();
	REQUIRE(noexceptSignature != nullptr);
	CHECK(noexceptSignature->IsNoexcept());

	CHECK(PgE::TypeOf<int(const char*, ...)>().GetSignature()->IsVariadic());

	// An abominable function type carries cv and ref qualifiers, which are independent of the ellipsis; a
	// member-pointer pointee is where one is actually reached.
	CHECK(PgE::TypeOf<int(const char*, ...) const>().GetSignature()->IsVariadic());
	CHECK(PgE::TypeOf<int(const char*, ...) const & noexcept>().GetSignature()->IsVariadic());
	CHECK_FALSE(PgE::TypeOf<int(const char*) const>().GetSignature()->IsVariadic());

	// The signature is reached through a pointer, which is how a callback field is actually spelled.
	const PgE::TypeInfo& functionPointer = PgE::TypeOf<void (*)(int, float)>();
	CHECK(functionPointer.GetKind() == PgE::TypeKind::Pointer);
	CHECK(&functionPointer.GetInnerType() == &function);

	CHECK(PgE::TypeOf<Inner>().GetSignature() == nullptr);
}

TEST_CASE("a member pointer decomposes into its class and its pointee")
{
	const PgE::TypeInfo& memberObject = PgE::TypeOf<int Inner::*>();
	CHECK(memberObject.GetKind() == PgE::TypeKind::MemberObjectPointer);

	const PgE::MemberPointerInfo* objectParts = memberObject.GetMemberPointer();
	REQUIRE(objectParts != nullptr);
	CHECK(&objectParts->GetClassType() == &PgE::TypeOf<Inner>());
	CHECK(&objectParts->GetPointeeType() == &PgE::TypeOf<int>());

	// A member function pointer's pointee is itself a function type, so the two decompositions compose
	// rather than the member pointer having to restate the signature.
	const PgE::TypeInfo& memberFunction = PgE::TypeOf<int (Inner::*)(double) const>();
	CHECK(memberFunction.GetKind() == PgE::TypeKind::MemberFunctionPointer);

	const PgE::MemberPointerInfo* functionParts = memberFunction.GetMemberPointer();
	REQUIRE(functionParts != nullptr);
	CHECK(&functionParts->GetClassType() == &PgE::TypeOf<Inner>());

	const PgE::TypeInfo& pointee = functionParts->GetPointeeType();
	CHECK(pointee.GetKind() == PgE::TypeKind::Function);

	const PgE::FunctionSignatureInfo* signature = pointee.GetSignature();
	REQUIRE(signature != nullptr);
	CHECK(&signature->GetReturnType() == &PgE::TypeOf<int>());
	REQUIRE(signature->GetParameters().size() == 1);
	CHECK(&signature->GetParameters()[0].Get() == &PgE::TypeOf<double>());

	CHECK(PgE::TypeOf<Inner*>().GetMemberPointer() == nullptr);
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

TEST_CASE("a volatile field is distinguished from its decayed tag")
{
	// volatile rounds out the cv pair alongside const: the stored TypeReference is decayed, so only the flag
	// separates a volatile-qualified member from an unqualified one that shares the same tag.
	const PgE::TypeInfo& type = PgE::TypeOf<VolatileHolder>();

	const PgE::FieldInfo& reg = FieldNamed(type, "Register");
	CHECK(reg.IsVolatile());
	CHECK_FALSE(reg.IsConst());

	const PgE::FieldInfo& plain = FieldNamed(type, "Plain");
	CHECK_FALSE(plain.IsVolatile());

	// Both decay to the same int tag; the qualifier flag is the only thing telling them apart.
	CHECK(&reg.GetTypeInfo() == &plain.GetTypeInfo());
	CHECK(&reg.GetTypeInfo() == &PgE::TypeOf<int>());
}

TEST_CASE("a defaulted special member is told apart from a user-provided one")
{
	// Constructors: Point() = default is compiler-generated; the explicit converting ctor is hand-written.
	const PgE::TypeInfo& point = PgE::TypeOf<Point>();

	const PgE::ConstructorInfo* defaulted = point.FindConstructor(PgE::ConstructorKind::Default);
	REQUIRE(defaulted != nullptr);
	CHECK(defaulted->IsDefaulted());

	const PgE::ConstructorInfo* converting = point.FindConstructor(PgE::ConstructorKind::Converting);
	REQUIRE(converting != nullptr);
	CHECK_FALSE(converting->IsDefaulted());

	// Operators: Comparable's operator== is = default; Coord's is hand-written. The flag reaches both through
	// the shared FunctionTraits, since an OperatorInfo is a FunctionInfo.
	const auto generated = PgE::TypeOf<Comparable>().FindOperators(PgE::OperatorKind::Equal);
	REQUIRE(generated.size() == 1);
	CHECK(generated.front()->IsDefaulted());

	const auto handWritten = PgE::TypeOf<Coord>().FindOperators(PgE::OperatorKind::Equal);
	REQUIRE(handWritten.size() == 1);
	CHECK_FALSE(handWritten.front()->IsDefaulted());
}

TEST_CASE("a type's destructor is reflected with its lifetime-end facts")
{
	// A trivial destructor: compiler-generated, trivial, non-virtual, and destroyable.
	const PgE::DestructorInfo& trivial = PgE::TypeOf<Widget>().GetDestructor();
	CHECK(trivial.CanDestroy());
	CHECK(trivial.IsTrivial());
	CHECK(trivial.IsDefaulted());
	CHECK_FALSE(trivial.IsVirtual());

	// A user-written destructor: not trivial, not defaulted.
	const PgE::DestructorInfo& userWritten = PgE::TypeOf<Destructible>().GetDestructor();
	CHECK(userWritten.CanDestroy());
	CHECK_FALSE(userWritten.IsTrivial());
	CHECK_FALSE(userWritten.IsDefaulted());

	// A defaulted destructor on a non-trivial type: defaulted, but not trivial (the string member decides).
	const PgE::DestructorInfo& defaulted = PgE::TypeOf<DefaultedDestructor>().GetDestructor();
	CHECK(defaulted.IsDefaulted());
	CHECK_FALSE(defaulted.IsTrivial());

	// A virtual destructor reports IsVirtual, which the whole-type HasVirtualDestructor also carries.
	const PgE::DestructorInfo& virtualDtor = PgE::TypeOf<Base>().GetDestructor();
	CHECK(virtualDtor.IsVirtual());
	CHECK(virtualDtor.IsDefaulted());

	// A deleted destructor reflects with no destroy thunk, the reason stated rather than a silent absence.
	const PgE::DestructorInfo& deleted = PgE::TypeOf<DeletedDestructor>().GetDestructor();
	CHECK(deleted.IsDeleted());
	CHECK_FALSE(deleted.CanDestroy());
}

TEST_CASE("the reflected destructor runs the real destructor")
{
	// The metadata did not replace the runtime: destroying through the DestructorInfo still ends the object's
	// lifetime, observed through the flag the destructor sets.
	bool destroyed = false;
	alignas(Destructible) std::byte storage[sizeof(Destructible)];
	Destructible* object = std::construct_at(reinterpret_cast<Destructible*>(storage), Destructible{.Flag = &destroyed});

	PgE::TypeOf<Destructible>().GetDestructor().Destroy(object);
	CHECK(destroyed);
}

TEST_CASE("nested types and member aliases are reflected as declared members")
{
	const PgE::TypeInfo& type = PgE::TypeOf<NestedOwner>();
	REQUIRE(type.GetNestedTypes().size() == 4);

	// A real nested definition: not an alias, and its reference resolves to the same TypeInfo TypeOf reaches
	// directly, so a consumer walking nested types lands on the canonical instance.
	const PgE::NestedTypeInfo* config = type.FindNestedType("Config");
	REQUIRE(config != nullptr);
	CHECK_FALSE(config->IsAlias());
	CHECK(&config->GetTypeInfo() == &PgE::TypeOf<NestedOwner::Config>());

	// A nested enum is a type member too.
	const PgE::NestedTypeInfo* season = type.FindNestedType("Season");
	REQUIRE(season != nullptr);
	CHECK_FALSE(season->IsAlias());
	CHECK(season->GetTypeInfo().GetKind() == PgE::TypeKind::Enum);

	// A member type-alias keeps its own name but resolves to the underlying type.
	const PgE::NestedTypeInfo* valueAlias = type.FindNestedType("ValueAlias");
	REQUIRE(valueAlias != nullptr);
	CHECK(valueAlias->IsAlias());
	CHECK(&valueAlias->GetTypeInfo() == &PgE::TypeOf<int>());

	// A self-alias resolves back to the enclosing type; the lazy reference is what keeps that from recursing
	// during construction.
	const PgE::NestedTypeInfo* selfAlias = type.FindNestedType("SelfAlias");
	REQUIRE(selfAlias != nullptr);
	CHECK(selfAlias->IsAlias());
	CHECK(&selfAlias->GetTypeInfo() == &type);
}

TEST_CASE("a type with no nested types reports none, with no injected class name")
{
	// members_of would surface the injected class name if it were not excluded; the empty result is what
	// confirms a plain is_type filter does not need to guard against it.
	CHECK(PgE::TypeOf<Widget>().GetNestedTypes().empty());
}

TEST_CASE("an anonymous nested type is excluded, so no entry is nameless")
{
	// `struct { int a; } field;` contributes an unnamed type member. Reflecting it would put a nameless entry
	// in the list that nothing can name and a lookup by empty identifier would match.
	const PgE::TypeInfo& type = PgE::TypeOf<AnonymousNested>();
	REQUIRE(type.GetNestedTypes().size() == 1);
	CHECK(type.GetNestedTypes().front().GetIdentifier() == "Named");
	CHECK(type.FindNestedType("") == nullptr);

	// The anonymous type is still reachable the ordinary way, through the field that uses it.
	const PgE::FieldInfo& unnamed = FieldNamed(type, "Unnamed");
	CHECK(unnamed.GetTypeInfo().GetIdentifier().empty());
}

TEST_CASE("a volatile class-typed field reflects with no accessors rather than breaking the build")
{
	// The decayed type is copyable and assignable, but the member is not: no copy constructor binds a volatile
	// lvalue. Guarding on the decayed type emitted thunks whose bodies were ill-formed, failing the whole type.
	const PgE::TypeInfo& type = PgE::TypeOf<VolatileClassHolder>();

	const PgE::FieldInfo& item = FieldNamed(type, "Item");
	CHECK(item.IsVolatile());

	VolatileClassHolder holder{};
	CHECK(item.GetAs<VolatileClassHolder::Payload>(&holder).error().Reason == PgE::FieldError::NotReadable);
	CHECK(item.SetAs<VolatileClassHolder::Payload>(&holder, {}).error().Reason == PgE::FieldError::NotWritable);

	// The borrow survives, since taking the member's address is unaffected by its volatility.
	CHECK(item.GetRef(static_cast<void*>(&holder)).has_value());

	// A volatile scalar keeps both accessors: only the class-typed case loses them.
	const PgE::FieldInfo& scalar = FieldNamed(PgE::TypeOf<VolatileHolder>(), "Register");
	VolatileHolder scalarHolder{};
	CHECK(scalar.SetAs<int>(&scalarHolder, 7).has_value());
	CHECK(*scalar.GetAs<int>(&scalarHolder) == 7);
}

TEST_CASE("a free function reflects by being named, and reports no access rather than private")
{
	const PgE::FunctionInfo& spawn = PgE::detail::FunctionOfMeta<^^FreeSpawn>();

	CHECK(spawn.GetIdentifier() == "FreeSpawn");
	REQUIRE(spawn.GetScopePath().size() == 1);
	CHECK(spawn.GetScopePath()[0] == "ReflectionTestTypes");

	// Access is a class-member notion. Reporting Private here would make a consumer that filters on Public
	// skip every free function, which is the defect AccessKind::None exists to prevent.
	CHECK(spawn.GetAccess() == PgE::AccessKind::None);

	// A free function is not a static member: it is not scoped to a class, and a projection places the two
	// differently. Both ignore the object pointer, which is what IsConstCallable reports.
	CHECK(spawn.IsFreeFunction());
	CHECK_FALSE(spawn.IsStatic());
	CHECK(spawn.IsConstCallable());

	REQUIRE(spawn.GetParams().size() == 2);
	CHECK(&spawn.GetParams()[0].GetTypeInfo() == &PgE::TypeOf<int>());
	CHECK(&spawn.GetReturnType() == &PgE::TypeOf<int>());

	// The erased call works through the same thunk as a static member, with no object.
	const auto result = spawn.InvokeAs<int>(static_cast<void*>(nullptr), 4, 2.5F);
	REQUIRE(result.has_value());
	CHECK(*result == 10);

	// One instance per function, so a consumer can compare by pointer identity the way it does for types.
	CHECK(&PgE::detail::FunctionOfMeta<^^FreeSpawn>() == &spawn);
}

TEST_CASE("a namespace-scope variable reflects as a static field, keeping the constant-readable split")
{
	const PgE::StaticFieldInfo& counter = PgE::detail::VariableOfMeta<^^FreeCounter>();

	CHECK(counter.GetIdentifier() == "FreeCounter");
	CHECK(counter.GetAccess() == PgE::AccessKind::None);
	CHECK(&counter.GetTypeInfo() == &PgE::TypeOf<int>());

	// An addressable variable is settable, exactly as a static data member is.
	CHECK_FALSE(counter.IsConstantReadable());
	REQUIRE(counter.GetAs<int>().has_value());
	CHECK(counter.SetAs(11).has_value());
	CHECK(*counter.GetAs<int>() == 11);
	CHECK(counter.SetAs(7).has_value());

	// A constexpr variable has no out-of-line definition, so its address is never taken: it is captured by
	// value at consteval and reflects as read-only, which is what keeps it from failing at link time.
	const PgE::StaticFieldInfo& maxSlots = PgE::detail::VariableOfMeta<^^FreeMaxSlots>();
	CHECK(maxSlots.IsConstantReadable());
	CHECK(*maxSlots.GetAs<int>() == 8);
	CHECK(maxSlots.SetAs(9).error().Reason == PgE::FieldError::NotWritable);
	CHECK(maxSlots.GetRef().error().Reason == PgE::FieldError::NotAddressable);
}
