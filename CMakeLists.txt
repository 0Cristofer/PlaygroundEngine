# Enables c++20 modules support. UUID specific to cmake 4.4 https://github.com/Kitware/CMake/blob/v4.4.0/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")

cmake_minimum_required(VERSION 4.3)

project(PlaygroundEngineMain)

# No in-source builds
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR
			"In-source builds are not supported.\n"
			"Create a build directory and run CMake from there.\n"
			"You may need to delete CMakeCache.txt and CMakeFiles/ first."
	)
endif ()

# Modules and "import std"
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

# Make sure cmake uses the same flag for all compilation steps, including internal ones, since otherwise, we might get mismatched types
set(CMAKE_CXX_STANDARD          26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# Helps clangd/tidy, no effect in builds
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensures we have a config defined in single-config generators
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	message(STATUS "[PlaygroundEngine] CMAKE_BUILD_TYPE not set — defaulting to Debug")
endif ()

include(CheckIPOSupported)
check_ipo_supported(RESULT PGE_IPO_SUPPORTED OUTPUT _PGE_IPO_OUTPUT LANGUAGES CXX)
if (NOT PGE_IPO_SUPPORTED)
	message(STATUS "[PlaygroundEngine] IPO/LTO not available: ${_PGE_IPO_OUTPUT}")
endif ()

# IPO/LTO is force-disabled pending a GCC modules+LTO bug: when a class's key function
# (first non-inline virtual) lives in a separate module implementation unit from its
# interface unit, LTO drops the class vtable, giving "undefined reference to vtable
# for ...@<module>" at link (hit by TransformComponent). Reproduces with a 3-file
# module-class-in-a-static-lib repro on plain g++ -flto -- not a build-config issue
# (gcc-ar and -ffat-lto-objects don't help; non-module or no-LTO links fine).
# Re-enable once it's fixed upstream, or once components shed polymorphism under ECS.
if (PGE_IPO_SUPPORTED)
	message(STATUS "[PlaygroundEngine] IPO/LTO supported but disabled (GCC modules+LTO vtable bug)")
	set(PGE_IPO_SUPPORTED OFF)
endif ()

# Ensures import std is correctly understood by CLion/Rider. Should be removed when better support comes
if (LINUX)
	# The std module sources must come from the active toolchain — a version mismatch
	# (e.g. the system libstdc++'s std.cc against this compiler's headers) fails to build.
	get_filename_component(_gccBinDir "${CMAKE_CXX_COMPILER}" DIRECTORY)
	file(GLOB _stdModuleSource "${_gccBinDir}/../include/c++/*/bits/std.cc")
	if (_stdModuleSource)
		list(GET _stdModuleSource 0 _stdModuleSource)
		get_filename_component(_stdModuleDir "${_stdModuleSource}" DIRECTORY)
		# EXCLUDE_FROM_ALL: the target only exists so the IDE indexes the std module
		# sources. Building it would provide a second 'std' module by-name, clashing
		# with the one CMake's own CXX_MODULE_STD support already builds per target.
		add_library(unused_std_target STATIC EXCLUDE_FROM_ALL)
		target_compile_features(unused_std_target PRIVATE cxx_std_23)
		target_sources(unused_std_target
				PRIVATE
				FILE_SET CXX_MODULES
				BASE_DIRS "${_stdModuleDir}"
				FILES "${_stdModuleDir}/std.cc" "${_stdModuleDir}/std.compat.cc"
		)
		target_compile_options(unused_std_target PRIVATE
				$<$<CXX_COMPILER_ID:GNU>:-w>
				$<$<CXX_COMPILER_ID:Clang>:-w>
		)
	else ()
		message(STATUS "[PlaygroundEngine] std module source not found — 'import std' may not resolve in IDE")
	endif ()
endif ()

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
)

enable_testing()

add_subdirectory(PlaygroundEngine)
add_subdirectory(PlaygroundGame)
add_subdirectory(PlaygroundReflection)
add_subdirectory(PlaygroundTests)
add_subdirectory(PlaygroundBenchmark)
