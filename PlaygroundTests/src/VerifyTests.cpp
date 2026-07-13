#include <doctest/doctest.h>

#include "PlaygroundEngine/Verify.h"

import PlaygroundEngine.Log;

// PGE_VERIFY's failure path logs and aborts, which cannot be caught in-process, so this pins the
// half that is observable: a satisfied condition is a no-op and execution continues past it.
TEST_CASE("PGE_VERIFY continues past a satisfied condition")
{
	bool reached = false;
	PGE_VERIFY(1 + 1 == 2);
	reached = true;

	CHECK(reached);
}
