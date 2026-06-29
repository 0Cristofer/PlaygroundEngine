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

TEST_CASE("scratch" * doctest::skip())
{
    // Replace with whatever you're poking at. Example:
    PGE_LOG(Info, "scratch reached; reflected name: {}", PgE::TypeOf<int>().GetDisplayName());
}
