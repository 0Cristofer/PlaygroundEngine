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
import PlaygroundEngine.Window;

TEST_CASE("scratch" * doctest::skip())
{
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
