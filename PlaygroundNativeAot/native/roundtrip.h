#pragma once

#include <cstdio>

// Everything below is identical for both backends. Only how a host obtains the
// two function pointers differs (dlopen vs hostfxr); once resolved, the boundary
// is the same C-ABI contract.

struct Position
{
	float x;
	float y;
	float z;
};

struct NativeHooks
{
	void (*log)(const char* text, int length);
};

using InitializeFn = void (*)(NativeHooks*);
using TickPositionsFn = void (*)(Position*, int);

inline void HostLog(const char* text, int length)
{
	std::printf("%.*s\n", length, text);
}

// Runs the whole round trip against two already-resolved entry points and
// returns 0 on success.
inline int RunRoundTrip(InitializeFn initialize, TickPositionsFn tickPositions)
{
	NativeHooks hooks{.log = &HostLog};
	initialize(&hooks);

	Position positions[3] = {{0, 0, 0}, {10, 10, 10}, {20, 20, 20}};
	std::printf("[C++] before: p0=(%.1f,%.1f,%.1f) p2=(%.1f,%.1f,%.1f)\n", positions[0].x, positions[0].y, positions[0].z, positions[2].x,
				positions[2].y, positions[2].z);

	tickPositions(positions, 3);

	std::printf("[C++] after:  p0=(%.1f,%.1f,%.1f) p2=(%.1f,%.1f,%.1f)\n", positions[0].x, positions[0].y, positions[0].z, positions[2].x,
				positions[2].y, positions[2].z);

	bool roundTripOk = positions[0].x == 1.0f && positions[2].z == 23.0f;
	std::printf("[C++] native<->managed round trip %s\n", roundTripOk ? "OK" : "FAILED");
	return roundTripOk ? 0 : 1;
}
