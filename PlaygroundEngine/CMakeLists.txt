cmake_minimum_required(VERSION 4.3)

add_library(PlaygroundEngine)

target_compile_features(PlaygroundEngine PUBLIC cxx_std_26)

target_compile_definitions(
	PlaygroundEngine
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

target_compile_options(
	PlaygroundEngine
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
	PlaygroundEngine
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			ON
		CMAKE_LINK_WARNING_AS_ERROR
			ON
		MSVC_RUNTIME_CHECKS
			"$<$<CONFIG:Debug>:StackFrameErrorCheck;UninitializedVariable>"
)

if(PGE_IPO_SUPPORTED)
	set_target_properties(
		PlaygroundEngine
		PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION_RELEASE
				ON
	)
endif()

include(FetchContent)

# --- spdlog setup --
FetchContent_Declare(
	spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG b8944a4
	SYSTEM
	FIND_PACKAGE_ARGS
		CONFIG
		NAMES
			spdlog
)

set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog)

target_compile_definitions(spdlog INTERFACE SPDLOG_HEADER_ONLY)
# --- spdlog setup --

# --- GLFW setup --
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 SYSTEM FIND_PACKAGE_ARGS NAMES glfw3)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)
# --- GLFW setup --

# --- GLM setup --
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.3 SYSTEM FIND_PACKAGE_ARGS NAMES glm)

# GLM_BUILD_LIBRARY OFF keeps glm::glm a pure INTERFACE target. The compiled variant would give the
# global -freflection a C++17 sub-target to leak onto, and nothing here needs the prebuilt symbols.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm)

# GLM defaults to OpenGL's [-1, 1] clip-space depth; Vulkan's is [0, 1]. Without this every
# projection matrix is silently wrong in Z. Carried on the glm target itself rather than on
# PlaygroundEngine, so every TU that reaches glm gets the same convention: this macro changes the
# definition of glm's inline functions, so a TU disagreeing about it is an ODR violation, not just a
# wrong matrix.
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)
# --- GLM setup --

# --- Vulkan setup --
# The C++ bindings ship as a named module rather than a header, so they get their own target. The
# configuration macros below must be set while that module is compiled: they bake into the BMI, and
# importers consequently need none of them. CMake's scanner orders vulkan.cppm after the :video
# partition it exports, so the two files are listed without any manual ordering.
find_package(Vulkan REQUIRED)

add_library(VulkanModule STATIC)

target_compile_features(VulkanModule PUBLIC cxx_std_26)

target_link_libraries(VulkanModule PUBLIC Vulkan::Vulkan)

target_compile_definitions(
	VulkanModule
	PRIVATE
		# The renderer is native runtime code, which must not depend on unwinding. USE_STD_EXPECTED
		# is what turns the vk::raii factories into std::expected<T, vk::Result>; defining
		# VULKAN_HPP_EXPECTED directly is not the supported switch.
		VULKAN_HPP_NO_EXCEPTIONS
		VULKAN_HPP_USE_STD_EXPECTED
		# Without this the vk:: structs gain constructors, making them non-aggregates, and
		# designated initializers stop compiling.
		VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
		# Defined empty on purpose. With exceptions off, resultCheck() otherwise asserts on any
		# non-success Result, aborting before the caller can read the returned std::expected.
		VULKAN_HPP_ASSERT_ON_RESULT=
)

# Generated third-party source, which does not compile clean under the engine's warning set.
target_compile_options(VulkanModule PRIVATE -w)

set_target_properties(
	VulkanModule
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			OFF
)

target_sources(
	VulkanModule
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				"${Vulkan_INCLUDE_DIR}/vulkan"
			FILES
				"${Vulkan_INCLUDE_DIR}/vulkan/vulkan_video.cppm"
				"${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)
# --- Vulkan setup --

# --- Slang shader setup --
# slangc is not part of FindVulkan's component set, so it is located on its own rather than
# through find_package(Vulkan).
find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function(add_slang_shader_target TARGET)
	cmake_parse_arguments("SHADER" "" "OUTPUT" "SOURCES;ENTRY_POINTS" ${ARGN})

	set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/Shaders/${SHADER_OUTPUT})

	set(ENTRY_POINT_FLAGS "")
	foreach(ENTRY_POINT IN LISTS SHADER_ENTRY_POINTS)
		list(
			APPEND ENTRY_POINT_FLAGS
			-entry
			${ENTRY_POINT}
		)
	endforeach()

	# DEPFILE carries the imports slangc discovers, which DEPENDS cannot know: without it an edit
	# to an imported .slang leaves a stale .spv behind with the build reporting success.

	add_custom_command(
		OUTPUT
			${OUTPUT_FILE}
		COMMAND
			${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINT_FLAGS} -depfile
			${OUTPUT_FILE}.d -o ${OUTPUT_FILE}
		DEPENDS
			${SHADER_SOURCES}
		DEPFILE ${OUTPUT_FILE}.d
		COMMENT "Compiling Slang shaders for ${TARGET}"
		VERBATIM
	)

	add_custom_target(${TARGET} DEPENDS ${OUTPUT_FILE})

	# Published as a global property rather than a target property: consumers live in sibling
	# directories and need the path at configure time, where a $<TARGET_PROPERTY:> genex cannot be
	# evaluated.

	set_property(
		GLOBAL
		PROPERTY
			${TARGET}_OUTPUT_FILE
				${OUTPUT_FILE}
	)
endfunction()

add_slang_shader_target(
	PlaygroundEngineShaders
	OUTPUT shaders.spv
	SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/Renderer/Shaders/shaders.slang
	ENTRY_POINTS vertMain fragMain
)

add_dependencies(PlaygroundEngine PlaygroundEngineShaders)
# --- Slang shader setup --

target_link_libraries(
	PlaygroundEngine
	PUBLIC
		spdlog::spdlog
	PRIVATE
		glfw
		glm::glm
		VulkanModule
)

target_sources(
	PlaygroundEngine
	PRIVATE
		src/GameObject.cpp
		src/Log.cpp
		src/Diagnostics.cpp
		src/ContractHandler.cpp
		src/Engine.cpp
		src/World.cpp
		src/Component/ComponentBase.cpp
		src/Component/TransformComponent.cpp
		src/App.cpp
		src/main.cpp
		src/Files.cpp
		src/Paths.cpp
		src/Window.cpp
		src/Reflection/Core/TypeInfo.cpp
		src/Reflection/Core/FieldInfo.cpp
		src/Reflection/Core/FunctionInfo.cpp
		src/Reflection/Core/Core.cpp
		src/Reflection/Builtins/EnumerationFacet.cpp
		src/Renderer/RendererVulkan.cpp
		src/Renderer/VulkanUtils.cpp
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				src
			FILES
				src/GameObject.cppm
				src/Log.cppm
				src/Diagnostics.cppm
				src/Engine.cppm
				src/App.cppm
				src/World.cppm
				src/Files.cppm
				src/Paths.cppm
				src/Window.cppm
				src/WindowTypes.cppm
				src/WindowBackend.GLFW.cppm
				src/Component/Components.cppm
				src/Component/ComponentBase.cppm
				src/Component/TransformComponent.cppm
				src/Reflection/Core/Builders/MetaCommon.cppm
				src/Reflection/Core/Builders/AnnotationsBuilder.cppm
				src/Reflection/Core/Builders/FieldsBuilder.cppm
				src/Reflection/Core/Builders/StaticFieldsBuilder.cppm
				src/Reflection/Core/Builders/TemplateBuilder.cppm
				src/Reflection/Core/Builders/FunctionsBuilder.cppm
				src/Reflection/Core/Builders/OperatorsBuilder.cppm
				src/Reflection/Core/Builders/ConversionsBuilder.cppm
				src/Reflection/Core/Builders/BasesBuilder.cppm
				src/Reflection/Core/Builders/ArgumentBinding.cppm
				src/Reflection/Core/Builders/ConstructorsBuilder.cppm
				src/Reflection/Core/Builders/DestructorBuilder.cppm
				src/Reflection/Core/Builders/NestedTypesBuilder.cppm
				src/Reflection/Core/Builders/FunctionSignatureBuilder.cppm
				src/Reflection/Core/Builders/MemberPointerBuilder.cppm
				src/Reflection/Core/Builders/TraitsBuilder.cppm
				src/Reflection/Core/Builders/TypeBuilder.cppm
				src/Reflection/Core/Builders/NamespaceBuilder.cppm
				src/Reflection/Core/Builders/FacetsBuilder.cppm
				src/Reflection/Core/Facets.cppm
				src/Reflection/Builtins/Builders/EnumerationFacetBuilder.cppm
				src/Reflection/Builtins/Builders/EnumeratorsBuilder.cppm
				src/Reflection/Builtins/Builders/SequenceFacetBuilder.cppm
				src/Reflection/Builtins/Builders/StringFacetBuilder.cppm
				src/Reflection/Builtins/EnumerationFacet.cppm
				src/Reflection/Builtins/StringFacet.cppm
				src/Reflection/Builtins/SequenceFacet.cppm
				src/Reflection/Builtins/Builtins.cppm
				src/Reflection/Core/FieldInfo.cppm
				src/Reflection/Core/StaticFieldInfo.cppm
				src/Reflection/Core/TemplateInfo.cppm
				src/Reflection/Core/FunctionSignatureInfo.cppm
				src/Reflection/Core/MemberPointerInfo.cppm
				src/Reflection/Core/FunctionInfo.cppm
				src/Reflection/Core/OperatorInfo.cppm
				src/Reflection/Core/ConversionInfo.cppm
				src/Reflection/Core/BaseInfo.cppm
				src/Reflection/Core/ParameterInfo.cppm
				src/Reflection/Core/ConstructorInfo.cppm
				src/Reflection/Core/DestructorInfo.cppm
				src/Reflection/Core/NestedTypeInfo.cppm
				src/Reflection/Core/NamespaceInfo.cppm
				src/Reflection/Reflection.cppm
				src/Reflection/Contracts.cppm
				src/Reflection/Core/TypedRef.cppm
				src/Reflection/Core/TypeInfo.cppm
				src/Reflection/Core/TypeReference.cppm
				src/Reflection/Core/EntityInfo.cppm
				src/Reflection/Core/DeclarationInfo.cppm
				src/Reflection/Core/TypeInfoTraits.cppm
				src/Reflection/Core/Core.cppm
				src/Renderer/Vertex.cppm
				src/Renderer/VulkanTypes.cppm
				src/Renderer/VulkanUtils.cppm
				src/Renderer/RendererVulkan.cppm
	PUBLIC
		FILE_SET HEADERS
			BASE_DIRS
				include
			FILES
				include/PlaygroundEngine/Log.h
				include/PlaygroundEngine/EntryPoint.h
				include/PlaygroundEngine/Verify.h
)
