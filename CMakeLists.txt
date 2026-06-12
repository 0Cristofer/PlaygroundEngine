# Enables c++20 modules support. UUID specific to cmake 4.3 https://github.com/Kitware/CMake/blob/v4.3.0/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

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

# Global options to ensure all compilation (including cmake internal) steps are using the same flags
add_compile_options(
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
		$<$<CXX_COMPILER_ID:GNU>:-freflection>
)

add_subdirectory(PlaygroundEngine)
add_subdirectory(PlaygroundGame)
add_subdirectory(PlaygroundReflection)
