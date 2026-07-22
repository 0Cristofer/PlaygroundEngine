// JIT backend: the C# is a plain managed .dll. Bootstrap CoreCLR through
// hostfxr, then resolve the same [UnmanagedCallersOnly] methods into function
// pointers. Everything after that is identical to the AOT host.
#include <cstddef>
#include <dlfcn.h>

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

#include "roundtrip.h"

namespace
{
	struct Hostfxr
	{
		hostfxr_initialize_for_runtime_config_fn initializeForRuntimeConfig;
		hostfxr_get_runtime_delegate_fn getRuntimeDelegate;
		hostfxr_close_fn close;
	};

	// Locate libhostfxr via nethost, load it, pull the entry points we need.
	bool LoadHostfxr(Hostfxr& out)
	{
		char_t pathBuffer[1024];
		size_t bufferSize = sizeof(pathBuffer) / sizeof(char_t);
		if (get_hostfxr_path(pathBuffer, &bufferSize, nullptr) != 0)
		{
			std::fprintf(stderr, "[C++] get_hostfxr_path failed\n");
			return false;
		}

		void* library = dlopen(pathBuffer, RTLD_NOW | RTLD_LOCAL);
		if (library == nullptr)
		{
			std::fprintf(stderr, "[C++] dlopen(hostfxr) failed: %s\n", dlerror());
			return false;
		}

		out.initializeForRuntimeConfig =
			reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(dlsym(library, "hostfxr_initialize_for_runtime_config"));
		out.getRuntimeDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(dlsym(library, "hostfxr_get_runtime_delegate"));
		out.close = reinterpret_cast<hostfxr_close_fn>(dlsym(library, "hostfxr_close"));

		return out.initializeForRuntimeConfig != nullptr && out.getRuntimeDelegate != nullptr && out.close != nullptr;
	}
}

int main()
{
	Hostfxr hostfxr{};
	if (!LoadHostfxr(hostfxr))
	{
		return 1;
	}

	hostfxr_handle context = nullptr;
	if (hostfxr.initializeForRuntimeConfig("./ManagedEcs.runtimeconfig.json", nullptr, &context) != 0 || context == nullptr)
	{
		std::fprintf(stderr, "[C++] initialize_for_runtime_config failed\n");
		return 1;
	}

	load_assembly_and_get_function_pointer_fn loadAssemblyAndGetFunctionPointer = nullptr;
	if (hostfxr.getRuntimeDelegate(context, hdt_load_assembly_and_get_function_pointer,
								   reinterpret_cast<void**>(&loadAssemblyAndGetFunctionPointer)) != 0 ||
		loadAssemblyAndGetFunctionPointer == nullptr)
	{
		std::fprintf(stderr, "[C++] get_runtime_delegate failed\n");
		hostfxr.close(context);
		return 1;
	}

	const char_t* assembly = "./ManagedEcs.dll";
	const char_t* type = "ManagedEcs.Interop, ManagedEcs";

	InitializeFn initialize = nullptr;
	TickPositionsFn tickPositions = nullptr;

	// UNMANAGEDCALLERSONLY_METHOD resolves an [UnmanagedCallersOnly] method
	// straight to its native function pointer, no delegate needed.
	bool resolved = loadAssemblyAndGetFunctionPointer(assembly, type, "Initialize", UNMANAGEDCALLERSONLY_METHOD, nullptr,
													  reinterpret_cast<void**>(&initialize)) == 0 &&
					loadAssemblyAndGetFunctionPointer(assembly, type, "TickPositions", UNMANAGEDCALLERSONLY_METHOD, nullptr,
													  reinterpret_cast<void**>(&tickPositions)) == 0 &&
					initialize != nullptr && tickPositions != nullptr;

	if (!resolved)
	{
		std::fprintf(stderr, "[C++] load_assembly_and_get_function_pointer failed\n");
		hostfxr.close(context);
		return 1;
	}

	int result = RunRoundTrip(initialize, tickPositions);
	hostfxr.close(context);
	return result;
}
