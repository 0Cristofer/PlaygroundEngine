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
        src/Ecs/EntitySpawnerSystem/EntitySpawnerSystem.cpp
        src/PlaygroundGame.cpp
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				src
			FILES
				src/App.cppm
                src/Ecs/EntityNameComponent.cppm
                src/Ecs/EntitySpawnerSystem/EntitySpawnerSystem.cppm
	PUBLIC
		FILE_SET HEADERS
			BASE_DIRS
				include
			FILES
)

target_link_libraries(
	PlaygroundGame
	PRIVATE
		PlaygroundEngine
		ImGuiModule
)

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

# Textures are located the same way the shaders above are, so the images the engine publishes are
# staged into a Textures folder beside the executable. Same stand-in status, and the same reason for
# being a build edge rather than a POST_BUILD step: an asset-only edit does not relink the executable.

get_property(TEXTURE_SOURCES GLOBAL PROPERTY PlaygroundEngineTextures_SOURCE_FILES)

set(STAGED_TEXTURES "")

foreach(TEXTURE_SOURCE IN LISTS TEXTURE_SOURCES)
	get_filename_component(TEXTURE_NAME ${TEXTURE_SOURCE} NAME)
	set(STAGED_TEXTURE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Textures/${TEXTURE_NAME})

	add_custom_command(
		OUTPUT
			${STAGED_TEXTURE}
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different ${TEXTURE_SOURCE} ${STAGED_TEXTURE}
		DEPENDS
			${TEXTURE_SOURCE}
		COMMENT "Staging ${TEXTURE_NAME} next to PlaygroundGame"
		VERBATIM
	)

	list(APPEND STAGED_TEXTURES ${STAGED_TEXTURE})
endforeach()

add_custom_target(PlaygroundGameTextures DEPENDS ${STAGED_TEXTURES})

add_dependencies(PlaygroundGame PlaygroundGameTextures)

# Models are staged exactly like the textures above, into a Models folder beside the executable.

get_property(MODEL_SOURCES GLOBAL PROPERTY PlaygroundEngineModels_SOURCE_FILES)

set(STAGED_MODELS "")

foreach(MODEL_SOURCE IN LISTS MODEL_SOURCES)
	get_filename_component(MODEL_NAME ${MODEL_SOURCE} NAME)
	set(STAGED_MODEL ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/Models/${MODEL_NAME})

	add_custom_command(
		OUTPUT
			${STAGED_MODEL}
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different ${MODEL_SOURCE} ${STAGED_MODEL}
		DEPENDS
			${MODEL_SOURCE}
		COMMENT "Staging ${MODEL_NAME} next to PlaygroundGame"
		VERBATIM
	)

	list(APPEND STAGED_MODELS ${STAGED_MODEL})
endforeach()

add_custom_target(PlaygroundGameModels DEPENDS ${STAGED_MODELS})

add_dependencies(PlaygroundGame PlaygroundGameModels)
