cmake_minimum_required(VERSION 3.28)
project(PlaygroundNativeAot CXX)

# Standalone spike: the SAME C# source (managed/Interop.cs) is consumed by two
# backends, and a C++ host loads each. Only the C# build flags and the C++
# loader differ; the boundary (native/roundtrip.h) is shared verbatim.
#
#   AOT backend: C# -> native .so, host_aot loads it via dlopen/dlsym.
#   JIT backend: C# -> managed .dll, host_jit boots CoreCLR via hostfxr.

find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

# dotnet wants Debug/Release; map the engine's RelWithDebInfo onto Release.
set(publish_config $<IF:$<CONFIG:Debug>,Debug,Release>)

file(GLOB managed_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/managed/*.cs)

# --- AOT backend: C# published as a self-contained native shared library -------
set(aot_csproj ${CMAKE_CURRENT_SOURCE_DIR}/managed/ManagedEcs.csproj)
set(aot_dir ${CMAKE_CURRENT_BINARY_DIR}/managed-aot/$<CONFIG>)
set(aot_so ${aot_dir}/ManagedEcs.so)

add_custom_command(
	OUTPUT
		${aot_so}
	COMMAND
		${DOTNET_EXECUTABLE} publish ${aot_csproj} --configuration ${publish_config} --output ${aot_dir}
	DEPENDS
		${managed_sources}
		${aot_csproj}
	COMMENT "AOT: publishing C# as a native shared library (${publish_config})"
	VERBATIM
)
add_custom_target(ManagedEcsAot DEPENDS ${aot_so})

# --- JIT backend: same C# built as a plain managed assembly + runtimeconfig ----
set(jit_csproj ${CMAKE_CURRENT_SOURCE_DIR}/managed-jit/ManagedEcs.Jit.csproj)
set(jit_dir ${CMAKE_CURRENT_BINARY_DIR}/managed-jit/$<CONFIG>)
set(jit_dll ${jit_dir}/ManagedEcs.dll)

add_custom_command(
	OUTPUT
		${jit_dll}
	COMMAND
		${DOTNET_EXECUTABLE} build ${jit_csproj} --configuration ${publish_config} --output ${jit_dir}
	DEPENDS
		${managed_sources}
		${jit_csproj}
	COMMENT "JIT: building C# as a managed assembly (${publish_config})"
	VERBATIM
)
add_custom_target(ManagedEcsJit DEPENDS ${jit_dll})

# --- hostfxr/nethost from the installed .NET apphost pack ----------------------
file(GLOB _nethost_headers /usr/lib/dotnet/packs/Microsoft.NETCore.App.Host.*/*/runtimes/*/native/nethost.h)
if(NOT _nethost_headers)
	message(FATAL_ERROR "nethost.h not found in any installed .NET apphost pack")
endif()
list(SORT _nethost_headers)
list(GET _nethost_headers -1 _nethost_header)
get_filename_component(NETHOST_DIR ${_nethost_header} DIRECTORY)

# --- AOT host -----------------------------------------------------------------
add_executable(host_aot native/host_aot.cpp)
target_compile_features(host_aot PRIVATE cxx_std_23)
target_compile_options(
	host_aot
	PRIVATE
		-Wall
		-Wextra
)
target_link_libraries(host_aot PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(host_aot ManagedEcsAot)
add_custom_command(
	TARGET host_aot
	POST_BUILD
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${aot_so} $<TARGET_FILE_DIR:host_aot>/ManagedEcs.so
	VERBATIM
)

# --- JIT host -----------------------------------------------------------------
add_executable(host_jit native/host_jit.cpp)
target_compile_features(host_jit PRIVATE cxx_std_23)
target_compile_options(
	host_jit
	PRIVATE
		-Wall
		-Wextra
)
target_include_directories(host_jit PRIVATE ${NETHOST_DIR})
target_link_libraries(
	host_jit
	PRIVATE
		${NETHOST_DIR}/libnethost.a
		${CMAKE_DL_LIBS}
)
add_dependencies(host_jit ManagedEcsJit)
# Stage the managed .dll, its runtimeconfig, and deps beside the host.
add_custom_command(
	TARGET host_jit
	POST_BUILD
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${jit_dll} $<TARGET_FILE_DIR:host_jit>/ManagedEcs.dll
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${jit_dir}/ManagedEcs.runtimeconfig.json $<TARGET_FILE_DIR:host_jit>/ManagedEcs.runtimeconfig.json
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${jit_dir}/ManagedEcs.deps.json $<TARGET_FILE_DIR:host_jit>/ManagedEcs.deps.json
	VERBATIM
)

enable_testing()
add_test(NAME aot_roundtrip COMMAND host_aot)
set_tests_properties(
	aot_roundtrip
	PROPERTIES
		WORKING_DIRECTORY
			$<TARGET_FILE_DIR:host_aot>
		PASS_REGULAR_EXPRESSION
			"round trip OK"
)

add_test(NAME jit_roundtrip COMMAND host_jit)
set_tests_properties(
	jit_roundtrip
	PROPERTIES
		WORKING_DIRECTORY
			$<TARGET_FILE_DIR:host_jit>
		ENVIRONMENT
			"DOTNET_ROOT=/usr/lib/dotnet"
		PASS_REGULAR_EXPRESSION
			"round trip OK"
)
