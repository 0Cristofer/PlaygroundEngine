// Explicit test entry point. doctest supplies the test registry and runner; we own
// main() so future harness concerns — environment setup, custom reporters (e.g.
// TeamCity service messages for IDE integration), extra CLI flags — have a home.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main(const int argc, char** argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    const int result = context.run();

    return result;
}
