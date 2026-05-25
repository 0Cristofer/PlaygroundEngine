cmake_minimum_required(VERSION 4.3)

add_executable(PlaygroundGame)

target_compile_definitions(PlaygroundGame
    PRIVATE
        # Windows specifics to improve compile times
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
)

target_compile_options(PlaygroundGame
    PRIVATE
        # Enables all possible warnings
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/permissive->
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>

        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
)

set_target_properties(PlaygroundGame PROPERTIES
    COMPILE_WARNING_AS_ERROR ON
    CMAKE_LINK_WARNING_AS_ERROR ON
    MSVC_RUNTIME_CHECKS "$<$<CONFIG:Debug>:UninitializedVariable;StackFrameErrorCheck>"
)

if(PGE_IPO_SUPPORTED)
    set_target_properties(PlaygroundGame PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
endif()

target_sources(PlaygroundGame
    PRIVATE
        src/PlaygroundGame.cpp
)

target_link_libraries(PlaygroundGame
    PRIVATE
        PlaygroundEngine
)