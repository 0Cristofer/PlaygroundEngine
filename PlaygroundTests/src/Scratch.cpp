// Development scratchpad, NOT a real test: a hidden, skipped doctest case for running engine code by hand
// and watching log output (the harness links the engine and inits logging). Run via the "Scratchpad test"
// IDE config, or: ./build/linux/PlaygroundTests/Debug/PlaygroundTests --test-case=scratch --no-skip
#include <doctest/doctest.h>

#include "PlaygroundEngine/Log.h"

import std;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

namespace
{
	int Spawn(const int count, const float scale)
	{
		return static_cast<int>(count * scale);
	}

	int GlobalCounter = 7;
	constexpr int MaxSlots = 8;

	struct Widget
	{
		int Value = 0;
		int Scale(const double factor) const
		{
			return static_cast<int>(Value * factor);
		}
	};

	struct Callbacks
	{
		void (*OnClick)(int, float) = nullptr;
		int (*Compute)(double) noexcept = nullptr;
		int Widget::* ValuePointer = nullptr;
		int (Widget::*ScalePointer)(double) const = nullptr;
	};
}

TEST_CASE("scratch" * doctest::skip())
{
	const PgE::FunctionInfo& spawn = PgE::detail::FunctionOfMeta<^^Spawn>();
	PGE_LOG(Info, "Spawn: id={} free={} static={} access={} constCallable={} params={}", spawn.GetIdentifier(), spawn.IsFreeFunction(),
			spawn.IsStatic(), PgE::ToString(spawn.GetAccess()), spawn.IsConstCallable(), spawn.GetParams().size());

	const auto spawned = spawn.InvokeAs<int>(static_cast<void*>(nullptr), 4, 2.5F);
	PGE_LOG(Info, "  invoked -> has_value={} value={}", spawned.has_value(), spawned ? *spawned : -1);

	const PgE::StaticFieldInfo& counter = PgE::detail::VariableOfMeta<^^GlobalCounter>();
	PGE_LOG(Info, "GlobalCounter: id={} access={} constantReadable={} type={}", counter.GetIdentifier(), PgE::ToString(counter.GetAccess()),
			counter.GetTraits().IsConstantReadable, counter.GetTypeInfo().GetIdentifier());

	const auto counterValue = counter.GetAs<int>();
	const auto counterSet = counter.SetAs(11);
	PGE_LOG(Info, "  read={} wrote={} nowReads={}", counterValue.value_or(-1), counterSet.has_value(), counter.GetAs<int>().value_or(-1));

	const PgE::StaticFieldInfo& maxSlots = PgE::detail::VariableOfMeta<^^MaxSlots>();
	PGE_LOG(Info, "MaxSlots: constantReadable={} read={} settable={}", maxSlots.IsConstantReadable(), maxSlots.GetAs<int>().value_or(-1),
			maxSlots.SetAs(9).has_value());

	const PgE::TypeInfo& callbacks = PgE::TypeOf<Callbacks>();

	const PgE::TypeInfo& onClick = callbacks.FindFieldByIdentifier("OnClick")->GetTypeInfo();
	const PgE::TypeInfo& onClickFunction = onClick.GetInnerType();
	const PgE::FunctionSignatureInfo* onClickSignature = onClickFunction.GetSignature();

	PGE_LOG(Info, "OnClick: pointerKind={} innerKind={} hasSignature={}", PgE::ToString(onClick.GetKind()), PgE::ToString(onClickFunction.GetKind()),
			onClickSignature != nullptr);
	PGE_LOG(Info, "  returns {} takes {} parameter(s), noexcept={}", onClickSignature->GetReturnType().GetIdentifier(),
			onClickSignature->GetParameters().size(), onClickSignature->IsNoexcept());
	for (const PgE::TypeReference& parameter : onClickSignature->GetParameters())
	{
		PGE_LOG(Info, "  parameter: {}", parameter.Get().GetIdentifier());
	}

	const PgE::FunctionSignatureInfo* computeSignature = callbacks.FindFieldByIdentifier("Compute")->GetTypeInfo().GetInnerType().GetSignature();
	PGE_LOG(Info, "Compute: returns {} noexcept={}", computeSignature->GetReturnType().GetIdentifier(), computeSignature->IsNoexcept());

	const PgE::MemberPointerInfo* valuePointer = callbacks.FindFieldByIdentifier("ValuePointer")->GetTypeInfo().GetMemberPointer();
	PGE_LOG(Info, "ValuePointer: class={} pointee={}", valuePointer->GetClassType().GetIdentifier(), valuePointer->GetPointeeType().GetIdentifier());

	const PgE::MemberPointerInfo* scalePointer = callbacks.FindFieldByIdentifier("ScalePointer")->GetTypeInfo().GetMemberPointer();
	const PgE::TypeInfo& scalePointee = scalePointer->GetPointeeType();
	PGE_LOG(Info, "ScalePointer: class={} pointeeKind={} pointeeHasSignature={}", scalePointer->GetClassType().GetIdentifier(),
			PgE::ToString(scalePointee.GetKind()), scalePointee.GetSignature() != nullptr);
	if (const PgE::FunctionSignatureInfo* scaleSignature = scalePointee.GetSignature())
	{
		PGE_LOG(Info, "  returns {} takes {} parameter(s)", scaleSignature->GetReturnType().GetIdentifier(), scaleSignature->GetParameters().size());
	}
}
