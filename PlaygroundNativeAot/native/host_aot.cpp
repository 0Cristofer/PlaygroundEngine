// NativeAOT backend: the C# is a self-contained native .so. Load it and resolve
// the exported symbols directly. No runtime bootstrap.
#include <dlfcn.h>

#include "roundtrip.h"

int main()
{
	void* library = dlopen("./ManagedEcs.so", RTLD_NOW | RTLD_LOCAL);
	if (library == nullptr)
	{
		std::fprintf(stderr, "[C++] dlopen failed: %s\n", dlerror());
		return 1;
	}

	auto initialize = reinterpret_cast<InitializeFn>(dlsym(library, "Managed_Initialize"));
	auto tickPositions = reinterpret_cast<TickPositionsFn>(dlsym(library, "Managed_TickPositions"));
	if (initialize == nullptr || tickPositions == nullptr)
	{
		std::fprintf(stderr, "[C++] dlsym failed: %s\n", dlerror());
		return 1;
	}

	return RunRoundTrip(initialize, tickPositions);
}
