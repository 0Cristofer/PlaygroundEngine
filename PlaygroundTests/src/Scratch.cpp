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

    struct Holder { Movable Item; };
}

TEST_CASE("scratch" * doctest::skip())
{
    Holder holder{};
    const PgE::TypeInfo& type = PgE::TypeOf<Holder>();

    const auto valueGet = type.GetFieldAs<Movable>(&holder, "Item");
	PGE_LOG(Info, "value get: has_value={} reason={}", valueGet.has_value(),
			valueGet ? -1 : static_cast<int>(valueGet.error().Reason));

    Movable source;
    source.Tag = 5;
    const auto valueSet = type.SetFieldAs(&holder, "Item", source);
    PGE_LOG(Info, "value set: has_value={} reason={}", valueSet.has_value(),
            valueSet ? -1 : static_cast<int>(valueSet.error().Reason));

    auto borrow = type.GetFieldRefAs<Movable>(&holder, "Item");
    PGE_LOG(Info, "borrow: has_value={}", borrow.has_value());
    if (borrow)
    {
        Movable replacement;
        replacement.Tag = 42;
        borrow->get() = std::move(replacement);
        PGE_LOG(Info, "borrow move-assigned; Item.Tag={}", holder.Item.Tag);
    }
}
