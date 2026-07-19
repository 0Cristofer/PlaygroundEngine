#include <meta>
#include <doctest/doctest.h>
#include "PlaygroundEngine/Log.h"
import std;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

struct Tag
{};

struct Base
{
	int X = 1;
	float Y = 2.0f;
};

struct Derived : Tag, Base
{};

TEST_CASE("scratch" * doctest::skip())
{
	PGE_LOG(Info, "Derived: {}", PgE::ToString(Derived{}));
}
