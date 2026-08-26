#include <doctest/doctest.h>
#include "PlaygroundEngine/Log.h"
import std;
import PlaygroundEngine.Log;

TEST_CASE("scratch" * doctest::skip())
{
	PGE_LOG(Info, "scratch");
}
