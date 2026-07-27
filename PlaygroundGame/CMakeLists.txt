cmake_minimum_required(VERSION 4.3)

add_executable(PlaygroundGame)

target_compile_definitions(
	PlaygroundGame
	PRIVATE
		# Windows specifics to improve compile times
		$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
		$<$<PLATFORM_ID:Windows>:NOMINMAX>
		$<$<PLATFORM_ID:Windows>:UNICODE>
		$<$<PLATFORM_ID:Windows>:_UNICODE>
)

target_compile_options(
	PlaygroundGame
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

set_target_properties(
	PlaygroundGame
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			ON
		CMAKE_LINK_WARNING_AS_ERROR
			ON
		MSVC_RUNTIME_CHECKS
			"$<$<CONFIG:Debug>:UninitializedVariable;StackFrameErrorCheck>"
)

if(PGE_IPO_SUPPORTED)
	set_target_properties(
		PlaygroundGame
		PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION_RELEASE
				ON
	)
endif()

target_sources(
	PlaygroundGame
	PRIVATE
		src/App.cpp
		src/PlaygroundGame.cpp
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				src
			FILES
				src/App.cppm
	PUBLIC
		FILE_SET HEADERS
			BASE_DIRS
				include
			FILES
)

target_link_libraries(PlaygroundGame PRIVATE PlaygroundEngine)

# Shaders are loaded at runtime by a path relative to the working directory, so the compiled SPIR-V
# is staged into a Shaders folder beside the executable. A stand-in until the asset system owns
# content deployment.

# Staging is its own build edge rather than a POST_BUILD step: a shader-only edit leaves the engine
# archive untouched, so the executable never relinks and a POST_BUILD command would not run, leaving
# a stale .spv beside a successfully built binary.

get_property(COMPILED_SHADER GLOBAL PROPERTY PlaygroundEngineShaders_OUTPUT_FILE)

# $<CONFIG> mirrors where the multi-config generator places the executable. A TARGET_FILE_DIR genex
# would say it directly but cannot be evaluated in an OUTPUT, which has no associated target.

set(STAGED_SHADER ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Shaders/shaders.spv)

add_custom_command(
	OUTPUT
		${STAGED_SHADER}
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different ${COMPILED_SHADER} ${STAGED_SHADER}
	DEPENDS
		${COMPILED_SHADER}
	COMMENT "Staging shaders next to PlaygroundGame"
	VERBATIM
)

add_custom_target(PlaygroundGameShaders DEPENDS ${STAGED_SHADER})

add_dependencies(PlaygroundGame PlaygroundGameShaders)
