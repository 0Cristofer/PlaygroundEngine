# Enables c++20 modules support. UUID specific to cmake 4.4 https://github.com/Kitware/CMake/blob/v4.4.0/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

cmake_minimum_required(VERSION 4.3)

project(PlaygroundEngineMain)

# No in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(
		FATAL_ERROR
		"In-source builds are not supported.\n"
		"Create a build directory and run CMake from there.\n"
		"You may need to delete CMakeCache.txt and CMakeFiles/ first."
	)
endif()

# Activate the tracked git hooks at configure time. Git has no clone-time hook by design, so folding
# this into configure (which every developer runs before building) is the automatic path; it points
# core.hooksPath at scripts/hooks so the main-merge verify gate needs no manual setup step. Guarded to
# a real git checkout so a source tarball still configures. scripts/setup-hooks.sh is the manual equal.
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	find_program(GIT_EXECUTABLE git)
	if(GIT_EXECUTABLE)
		execute_process(
			COMMAND
				"${GIT_EXECUTABLE}" config core.hooksPath scripts/hooks
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			RESULT_VARIABLE _pgeHooksResult
		)
		if(_pgeHooksResult EQUAL 0)
			message(STATUS "[PlaygroundEngine] git hooks active: core.hooksPath -> scripts/hooks")
		else()
			message(WARNING "[PlaygroundEngine] could not set core.hooksPath; run scripts/setup-hooks.sh manually")
		endif()
	endif()
endif()

# Modules and "import std"
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

# Make sure cmake uses the same flag for all compilation steps, including internal ones, since otherwise, we might get mismatched types
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Helps clangd/tidy, no effect in builds
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensures we have a config defined in single-config generators
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	message(STATUS "[PlaygroundEngine] CMAKE_BUILD_TYPE not set: defaulting to Debug")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT PGE_IPO_SUPPORTED OUTPUT _PGE_IPO_OUTPUT LANGUAGES CXX)
if(NOT PGE_IPO_SUPPORTED)
	message(STATUS "[PlaygroundEngine] IPO/LTO not available: ${_PGE_IPO_OUTPUT}")
endif()

# IPO/LTO is force-disabled pending a GCC modules+LTO bug: when a class's key function
# (first non-inline virtual) lives in a separate module implementation unit from its
# interface unit, LTO drops the class vtable, giving "undefined reference to vtable
# for ...@<module>" at link (hit by TransformComponent). Reproduces with a 3-file
# module-class-in-a-static-lib repro on plain g++ -flto -- not a build-config issue
# (gcc-ar and -ffat-lto-objects don't help; non-module or no-LTO links fine).
# Re-enable once it's fixed upstream, or once components shed polymorphism under ECS.
if(PGE_IPO_SUPPORTED)
	message(STATUS "[PlaygroundEngine] IPO/LTO supported but disabled (GCC modules+LTO vtable bug)")
	set(PGE_IPO_SUPPORTED OFF)
endif()

# Ensures import std is correctly understood by CLion/Rider. Should be removed when better support comes
if(LINUX)
	# The std module sources must come from the active toolchain, since a version mismatch
	# (e.g. the system libstdc++'s std.cc against this compiler's headers) fails to build.
	get_filename_component(_gccBinDir "${CMAKE_CXX_COMPILER}" DIRECTORY)
	file(GLOB _stdModuleSource "${_gccBinDir}/../include/c++/*/bits/std.cc")
	if(_stdModuleSource)
		# An in-place toolchain upgrade leaves the old version's include tree (e.g. 16.1.1)
		# beside the new one (17.0.0), so the glob matches both. Natural-sort descending and
		# take the newest, otherwise a plain GET 0 picks the stale tree and the IDE indexes
		# std against headers that don't match the active compiler.
		list(SORT _stdModuleSource COMPARE NATURAL ORDER DESCENDING)
		list(GET _stdModuleSource 0 _stdModuleSource)
		get_filename_component(_stdModuleDir "${_stdModuleSource}" DIRECTORY)
		# EXCLUDE_FROM_ALL: the target only exists so the IDE indexes the std module
		# sources. Building it would provide a second 'std' module by-name, clashing
		# with the one CMake's own CXX_MODULE_STD support already builds per target.
		add_library(unused_std_target STATIC EXCLUDE_FROM_ALL)
		target_compile_features(unused_std_target PRIVATE cxx_std_23)
		target_sources(
			unused_std_target
			PRIVATE
				FILE_SET CXX_MODULES
					BASE_DIRS
						"${_stdModuleDir}"
					FILES
						"${_stdModuleDir}/std.cc"
						"${_stdModuleDir}/std.compat.cc"
		)
		target_compile_options(
			unused_std_target
			PRIVATE
				$<$<CXX_COMPILER_ID:GNU>:-w>
				$<$<CXX_COMPILER_ID:Clang>:-w>
		)
	else()
		message(STATUS "[PlaygroundEngine] std module source not found; 'import std' may not resolve in IDE")
	endif()

	# Embed the toolchain's runtime library dir as an rpath so binaries load this GCC's libstdc++,
	# not the older system one. The system libstdc++ lags the source-built GCC (e.g. it lacks the
	# GLIBCXX_3.4.36 that GCC 17 emits), so without this the dynamic loader picks the system copy
	# and load fails with "version GLIBCXX_... not found". add_link_options (not a preset linker
	# flag) so it survives the asan/coverage presets overriding CMAKE_EXE_LINKER_FLAGS.
	add_link_options("-Wl,-rpath,${_gccBinDir}/../lib64")
endif()

# Global options to ensure all compilation (including cmake internal) steps are using the same flags.
# -freflection MUST stay global across C++: CMake builds the `import std` module via an internal
# target that reads these global options, and -freflection alters module declarations - building the
# std module BMI without it while consumers import it WITH it gives "conflicting type for imported
# declaration" (e.g. __mbstate_t). It cannot be scoped to first-party targets. It IS scoped to the
# C++ language (COMPILE_LANGUAGE:CXX) so C dependencies (e.g. GLFW) don't get the flag and warn
# "valid for C++/ObjC++ but not for C" on every TU; the std module target is C++, so it still applies.
# The flag still leaks onto C++ FetchContent deps; any such sub-target compiling below C++26 must be
# exempted case-by-case (see PlaygroundTests disabling doctest's static-main lib).
add_compile_options(
	$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:CXX>>:-freflection>
	# C++26 contracts. Kept global for the same reason as -freflection: the internal `import std`
	# module target reads these options, and a semantic mismatch between the std BMI and consumers
	# must be avoided, so the semantic stays uniform across every TU within a config. It rides the
	# build zone rather than a standalone knob: the dev configs (Debug, RelWithDebInfo) enforce,
	# shipping (Release) ignores. Telemetry (observe) and the -fno-exceptions runtime get their own
	# presets later (see docs/CorrectnessAndStandards.md Section 4).
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:CXX>>:-fcontracts>
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:CXX>,$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:-fcontract-evaluation-semantic=enforce>
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:-fcontract-evaluation-semantic=ignore>
)

enable_testing()

add_subdirectory(PlaygroundEngine)
add_subdirectory(PlaygroundGame)
add_subdirectory(PlaygroundReflection)
add_subdirectory(PlaygroundTests)
add_subdirectory(PlaygroundBenchmark)
