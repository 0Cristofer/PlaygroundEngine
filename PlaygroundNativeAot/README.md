# C#/C++ interop spike (two backends)

Throwaway validation of the C# <-> C++ boundary, driven entirely by CMake. The
**same** `managed/Interop.cs` is consumed by two backends to show that only the
compilation and the loader differ:

- **AOT** (`host_aot`): C# published as a self-contained native `.so`; the host
  loads it with `dlopen`/`dlsym`. This is the console/mobile-shaped path.
- **JIT** (`host_jit`): the identical C# built as a managed `.dll`; the host boots
  CoreCLR through `hostfxr`/`nethost` and resolves the same methods. This is the
  desktop/editor-shaped path.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=$HOME/gcc-16/bin/g++
cmake --build build      # builds both backends + both hosts
cd build && ctest        # aot_roundtrip + jit_roundtrip
```

## What it proves

- **One `cmake --build` produces everything.** Each backend's C# build is an
  `add_custom_command` whose artifact the matching host depends on and stages
  beside itself. Editing the shared `Interop.cs` re-triggers *both* builds; an
  unchanged tree does no work.
- **The C# source is identical across backends.** `managed-jit` links the very
  same `Interop.cs` (`<Compile Include="../managed/Interop.cs">`); only the csproj
  flags differ (`PublishAot`+`NativeLib=Shared` vs `EnableDynamicLoading`).
- **The C++ boundary is identical.** Both hosts include `native/roundtrip.h`
  verbatim; they differ *only* in how they obtain the two function pointers
  (`native/host_aot.cpp` dlopen vs `native/host_jit.cpp` hostfxr).
- A managed "system" (`TickPositions`) mutates **native-owned component storage**
  in place through a raw pointer (the ECS shape in miniature); managed code calls
  back into C++ through a struct of native function pointers (the "hooks").

## Findings (.NET 10.0.110, GCC, WSL2 linux-x64)

- **Both backends pass**, same C# source, same output.
- **No clang required** for the AOT link; gcc/ld sufficed on .NET 10.
- **Artifact sizes tell the story:** AOT `.so` is **1.1 MB** (native code + the
  runtime linked in, depends only on libc/libm/vdso); JIT `.dll` is **5.5 KB**
  (just IL, the runtime is external and hostfxr loads it).
- **Startup cost is visible:** the AOT round trip clocks ~0.00 s, the JIT one
  ~0.03 s, that delta is CoreCLR bootstrapping.
- `host_jit` links `libnethost.a` statically from the installed apphost pack; at
  runtime `DOTNET_ROOT=/usr/lib/dotnet` lets `get_hostfxr_path` find the runtime.

## IDE entry points (two, by task)

The C++ and C# sides are authored as separate first-class projects over one repo,
because Rider's project model is either CMake mode or .NET-solution mode, never
both live at once.

- **C++ work:** open `CMakeLists.txt` (CMake mode) for full C++ services.
- **C# work:** open `managed/ManagedEcs.slnx` (.NET solution mode) for full C#
  IntelliSense / debugger / NuGet. Fast `dotnet build` JIT loop; CMake is not in
  the way.

They meet only at the build (`dotnet publish` custom command) and at runtime
(the dlopen C-ABI), not at the authoring/solution level. VS Code is the one
editor that can host both language services in a single window, since it has no
single-solution-model constraint.

## Out of scope (deliberately)

- Reflection / source-generated marshalling (the real binding layer replaces the
  hand-written structs here).
- GC interaction across frames (host must not hold managed pointers; here all
  memory is native-owned, which sidesteps it).
- Console/mobile runtime packs (each needs its own research + toolchain).
