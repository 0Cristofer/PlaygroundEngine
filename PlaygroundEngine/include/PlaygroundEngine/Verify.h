#pragma once

#include <cstdlib>

#include "PlaygroundEngine/Log.h"

// Always-on assertion that survives even a build set to ignore contracts (the semantic is global per
// build). It is an assertion, not a guard: a false condition is a bug, so it logs and aborts, never
// branches for recovery. Consumers import PlaygroundEngine.Log, as any PGE_LOG user does.
#define PGE_VERIFY(condition)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(condition))                                                                                                                            \
		{                                                                                                                                            \
			PGE_LOG(Fatal, "PGE_VERIFY failed: {}", #condition);                                                                                     \
			std::abort();                                                                                                                            \
		}                                                                                                                                            \
	} while (false)
