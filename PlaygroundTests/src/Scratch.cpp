// Development scratchpad — NOT a real test. A hidden doctest case for running engine
// code by hand during development and watching log output, using the test harness as
// a ready-made execution environment (it already links the engine and inits logging).
//
// Skipped by default so normal and CI runs ignore it. Run it on demand via the shared
// "Scratchpad test" IDE run configuration (.run/), which builds, runs, and can debug it;
// or directly:
//   ./build/linux/PlaygroundTests/Debug/PlaygroundTests --test-case=scratch --no-skip
#include <doctest/doctest.h>

#include "PlaygroundEngine/Log.h"

import std;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;
import PlaygroundEngine.Window;

// ReSharper disable CppEnumeratorNeverUsed
namespace
{
	// Move-only but move-assignable (like unique_ptr / Poly): copy ctor deleted, move ops defaulted.
	struct Movable
	{
		int Tag = 0;
		Movable() = default;
		Movable(const Movable&) = delete;
		Movable(Movable&&) = default;
		Movable& operator=(Movable&&) = default;
	};

	struct Holder
	{
		Movable Item;
	};

	enum class Colors
	{
		Red = 1,
		Green = 2,
		Yellow = 3,
		Blue = 4
	};
}

// ReSharper restore CppEnumeratorNeverUsed

TEST_CASE("scratch" * doctest::skip())
{
	Holder holder{};
	const PgE::TypeInfo& type = PgE::TypeOf<Holder>();

	const auto valueGet = type.GetFieldAs<Movable>(&holder, "Item");
	PGE_LOG(Info, "value get: has_value={} reason={}", valueGet.has_value(),
	        valueGet ? "" : PgE::ToString(valueGet.error().Reason));

	Movable source;
	source.Tag = 5;
	const auto valueSet = type.SetFieldAs(&holder, "Item", source);
	PGE_LOG(Info, "value set: has_value={} reason={}", valueSet.has_value(),
	        valueSet ? "" : PgE::ToString(valueSet.error().Reason));

	auto borrow = type.GetFieldRefAs<Movable>(&holder, "Item");
	PGE_LOG(Info, "borrow: has_value={}", borrow.has_value());
	if (borrow)
	{
		Movable replacement;
		replacement.Tag = 42;
		borrow->get() = std::move(replacement);
		PGE_LOG(Info, "borrow move-assigned; Item.Tag={}", holder.Item.Tag);
	}

	const auto& typeOfColors = PgE::TypeOf<Colors>();
	constexpr auto color = Colors::Blue;
	PGE_LOG(Info, "color: {}", PgE::ToString(color));
	for (auto enumeratorInfo : typeOfColors.GetFacet<PgE::EnumerationFacet>()->GetEnumerators())
	{
		PGE_LOG(Info, "enumerator info name: {}", enumeratorInfo.GetIdentifier());
	}

	const auto& typeOfTypeInfo = PgE::TypeOf<PgE::TypeInfo>();
	PGE_LOG(Info, "type of typeOfTypeInfo: {}", PgE::ToString(typeOfTypeInfo));

	auto window = PgE::Window::Create(PgE::WindowSpecification{
		.Title = "Playground Window", .Width = 960, .Height = 540});

	if (!window)
	{
		PGE_LOG(Error, "Window creation failed: reason={}", static_cast<int>(window.error()));
		return;
	}

	PGE_LOG(Info, "Window up: {}x{} \"{}\"", (*window)->GetWidth(), (*window)->GetHeight(),
			(*window)->GetTitle());

	while (!(*window)->ShouldClose())
	{
		(*window)->PollEvents();
		(*window)->SwapBuffers();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	PGE_LOG(Info, "Window loop finished (shouldClose={})", (*window)->ShouldClose());
}
