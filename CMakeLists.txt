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
    if (EXISTS /usr/include/c++/15/bits)
        add_library(unused_std_target STATIC)
        target_compile_features(unused_std_target PRIVATE cxx_std_23)
        target_sources(unused_std_target
                PRIVATE
                FILE_SET CXX_MODULES
                BASE_DIRS /usr/include/c++/15/bits
                FILES /usr/include/c++/15/bits/std.cc /usr/include/c++/15/bits/std.compat.cc
        )
        target_compile_options(unused_std_target PRIVATE
                $<$<CXX_COMPILER_ID:GNU>:-w>
                $<$<CXX_COMPILER_ID:Clang>:-w>
        )
    else ()
        message(STATUS "[PlaygroundEngine] std module source not found — 'import std' may not resolve in IDE")
    endif ()
endif ()

# spdlog uses UTF8 by default, so we need to make sure cmake uses it too when compiling std modules
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

add_subdirectory(PlaygroundEngine)
add_subdirectory(PlaygroundGame)