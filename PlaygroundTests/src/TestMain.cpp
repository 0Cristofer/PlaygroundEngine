// Explicit test entry point. doctest supplies the test registry and runner; we own
// main() so future harness concerns — environment setup, custom reporters (e.g.
// TeamCity service messages for IDE integration), extra CLI flags — have a home.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

import PlaygroundEngine.Log;

int main(const int argc, char** argv)
{
    // The Engine ctor normally brings logging up; tests don't construct an Engine, so
    // do it here. Without this, PGE_LOG dereferences a null logger.
    PgE::Log::Init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    const int result = context.run();

    return result;
}
