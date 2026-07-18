// Development scratchpad, NOT a real test: a hidden, skipped doctest case for running engine code by hand
// and watching log output (the harness links the engine and inits logging). Run via the "Scratchpad test"
// IDE config, or: ./build/linux/PlaygroundTests/Debug/PlaygroundTests --test-case=scratch --no-skip
#include <meta>

#include <doctest/doctest.h>

#include "PlaygroundEngine/Log.h"

import std;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

namespace ScratchSweep
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

	int Spawn(const int count)
	{
		return count;
	}

	void Spawn()
	{}

	void Unique()
	{}

	int Counter = 7;
	constexpr int MaxSlots = 8;

	template <typename T>
	struct Grid
	{};

	template <typename T>
	void Generic(T)
	{}

	namespace Inner
	{
		struct Deep
		{};
	}

	inline namespace V1
	{
		struct InInline
		{};
	}

	namespace Alias = Inner;
}

TEST_CASE("scratch" * doctest::skip())
{
	const PgE::NamespaceInfo& sweep = PgE::NamespaceOf<^^ScratchSweep>();

	PGE_LOG(Info, "namespace {}: types={} functions={} variables={} namespaces={}", sweep.GetIdentifier(), sweep.GetTypes().size(),
			sweep.GetFunctions().size(), sweep.GetVariables().size(), sweep.GetNamespaces().size());

	for (const PgE::NestedTypeInfo& type : sweep.GetTypes())
	{
		PGE_LOG(Info, "  type {} alias={} resolves to {}", type.GetIdentifier(), type.IsAlias(), type.GetTypeInfo().GetIdentifier());
	}

	for (const PgE::FunctionInfo* function : sweep.GetFunctions())
	{
		PGE_LOG(Info, "  function {} free={} params={} deleted={}", function->GetIdentifier(), function->IsFreeFunction(),
				function->GetParams().size(), function->IsDeleted());
	}

	for (const PgE::StaticFieldInfo* variable : sweep.GetVariables())
	{
		PGE_LOG(Info, "  variable {} type={} constantReadable={} read={}", variable->GetIdentifier(), variable->GetTypeInfo().GetIdentifier(),
				variable->IsConstantReadable(), variable->GetAs<int>().value_or(-1));
	}

	for (const PgE::NamespaceInfo* nested : sweep.GetNamespaces())
	{
		PGE_LOG(Info, "  namespace {} types={} functions={}", nested->GetIdentifier(), nested->GetTypes().size(), nested->GetFunctions().size());
	}

	const auto unique =
		std::ranges::find_if(sweep.GetFunctions(), [](const PgE::FunctionInfo* function) { return function->GetIdentifier() == "Unique"; });
	PGE_LOG(Info, "identity: swept function == named function: {}", *unique == &PgE::detail::FunctionOfMeta<^^ScratchSweep::Unique>());
	PGE_LOG(Info, "identity: namespace alias == namespace: {}",
			&PgE::NamespaceOf<^^ScratchSweep::Alias>() == &PgE::NamespaceOf<^^ScratchSweep::Inner>());
}
