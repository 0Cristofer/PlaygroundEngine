# Enables c++20 modules support. UUID specific to cmake 4.3 https://github.com/Kitware/CMake/blob/v4.3.0/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

cmake_minimum_required(VERSION 4.3)

project(PlaygroundEngineMain)

# No in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported.\n"
        "Create a build directory and run CMake from there.\n"
        "You may need to delete CMakeCache.txt and CMakeFiles/ first."
    )
endif()

# Modules and "import std"
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

# Helps clangd/tidy, no effect in builds
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensures we have a config defined in single-config generators
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    message(STATUS "[PlaygroundEngine] CMAKE_BUILD_TYPE not set — defaulting to Debug")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT PGE_IPO_SUPPORTED OUTPUT _PGE_IPO_OUTPUT LANGUAGES CXX)
if(NOT PGE_IPO_SUPPORTED)
    message(STATUS "[PlaygroundEngine] IPO/LTO not available: ${_PGE_IPO_OUTPUT}")
endif()

add_subdirectory(PlaygroundEngine)
add_subdirectory(PlaygroundGame)
