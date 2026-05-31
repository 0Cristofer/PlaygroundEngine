cmake_minimum_required(VERSION 4.3)

add_library(PlaygroundEngine)

target_compile_features(PlaygroundEngine PUBLIC cxx_std_23)

target_compile_definitions(PlaygroundEngine
        PUBLIC
        $<$<CONFIG:Debug,RelWithDebInfo>:PGE_DEV>
        $<$<CONFIG:Release>:PGE_RELEASE>
        PRIVATE
        # Windows specifics to improve compile times
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
)

target_compile_options(PlaygroundEngine
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

set_target_properties(PlaygroundEngine PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
        CMAKE_LINK_WARNING_AS_ERROR ON
        MSVC_RUNTIME_CHECKS "$<$<CONFIG:Debug>:StackFrameErrorCheck;UninitializedVariable>"
)

if (PGE_IPO_SUPPORTED)
    set_target_properties(PlaygroundEngine PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
endif ()

include(FetchContent)

# --- spdlog setup --
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG b8944a4
        SYSTEM
        FIND_PACKAGE_ARGS
        CONFIG
        NAMES spdlog
)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog)
# --- spdlog setup --

target_link_libraries(PlaygroundEngine PRIVATE spdlog::spdlog)

target_sources(PlaygroundEngine
        PRIVATE
        src/GameObject.cpp
        src/TestClass.cpp
        src/Log.cpp
        src/PlaygroundEngine.cpp
        src/World.cpp
        src/Component/ComponentBase.cpp
        src/Component/TransformComponent.cpp

        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/PlaygroundEngine/GameObject.h
        include/PlaygroundEngine/Log.h
        include/PlaygroundEngine/PlaygroundEngine.h
        include/PlaygroundEngine/World.h
        include/PlaygroundEngine/Component/ComponentBase.h
        include/PlaygroundEngine/Component/TransformComponent.h

        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS src
        FILES
        src/TestClass.cppm
)
