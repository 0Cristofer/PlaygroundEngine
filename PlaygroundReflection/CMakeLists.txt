cmake_minimum_required(VERSION 4.3)

add_executable(PlaygroundReflection)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
    message(FATAL_ERROR
            "[PlaygroundEngine] Reflection requires GCC 16+.\n"
            "Current compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    )
endif ()

target_compile_features(PlaygroundReflection PRIVATE cxx_std_26)

target_compile_definitions(PlaygroundReflection
        PRIVATE
        # Windows specifics to improve compile times
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
)

target_compile_options(PlaygroundReflection
        PRIVATE
        # Enables all possible warnings
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/permissive->
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>

        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-freflection>
)

set_target_properties(PlaygroundReflection PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
        CMAKE_LINK_WARNING_AS_ERROR ON
        MSVC_RUNTIME_CHECKS "$<$<CONFIG:Debug>:UninitializedVariable;StackFrameErrorCheck>"
)

if (PGE_IPO_SUPPORTED)
    set_target_properties(PlaygroundReflection PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
endif ()

target_sources(PlaygroundReflection
        PRIVATE
        src/main.cpp

        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS src
        FILES

        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
)
