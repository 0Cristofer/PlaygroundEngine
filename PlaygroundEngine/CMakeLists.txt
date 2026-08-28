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

# --- stb setup --
# stb ships no CMakeLists and cuts no releases, so it is pinned by commit and SOURCE_SUBDIR names a
# directory that does not exist, which is what stops MakeAvailable from calling add_subdirectory.
FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
	SOURCE_SUBDIR does-not-exist
)

FetchContent_MakeAvailable(stb)

# The single-header implementation gets its own target for the same reason VulkanModule does: it is
# third-party C that does not survive the engine's warning set. Kept at C++26 deliberately, since the
# global -freflection breaks any C++ TU compiled below it.
add_library(StbImage STATIC src/Image/StbImageImplementation.cpp)

target_compile_features(StbImage PUBLIC cxx_std_26)

target_include_directories(StbImage SYSTEM PUBLIC "${stb_SOURCE_DIR}")

target_compile_options(StbImage PRIVATE -w)

# STBI_NO_STDIO and STBI_WRITE_NO_STDIO route every load and store through PlaygroundEngine.Files
# instead of stb's own file handling, keeping one I/O path and one file-error type.
target_compile_definitions(
	StbImage
	PUBLIC
		STBI_NO_STDIO
		STBI_ONLY_JPEG
		STBI_ONLY_PNG
		STBI_WRITE_NO_STDIO
)

set_target_properties(
	StbImage
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			OFF
		# Not a module unit, and the global CMAKE_CXX_SCAN_FOR_MODULES would have it scanned as one.
		CXX_SCAN_FOR_MODULES
			OFF
)
# --- stb setup --

# --- tinyobjloader setup --
# tinyobjloader ships a CMakeLists, but building it would mean overriding its CMAKE_CXX_STANDARD 11
# (which the global -freflection does not survive), forcing SYSTEM onto its plain INTERFACE include
# directory, and silencing its warnings: three reach-ins into someone else's target. In exchange it
# adds unconditional install() rules with no opt-out, an export(), a find_package(Sanitizers) and an
# uninstall target. SOURCE_SUBDIR names a directory that does not exist so MakeAvailable skips
# add_subdirectory, and its implementation source is compiled here instead.
FetchContent_Declare(tinyobjloader GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git GIT_TAG v2.0.0rc13 SOURCE_SUBDIR does-not-exist)

FetchContent_MakeAvailable(tinyobjloader)

# tiny_obj_loader.cc is upstream's own one-line implementation TU, so there is nothing to hand-write.
# Kept at C++26 because the global -freflection breaks any C++ TU compiled below it.
add_library(TinyObjLoader STATIC "${tinyobjloader_SOURCE_DIR}/tiny_obj_loader.cc")

target_compile_features(TinyObjLoader PUBLIC cxx_std_26)

target_include_directories(TinyObjLoader SYSTEM PUBLIC "${tinyobjloader_SOURCE_DIR}")

target_compile_options(TinyObjLoader PRIVATE -w)

set_target_properties(
	TinyObjLoader
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			OFF
		# Not a module unit, and the global CMAKE_CXX_SCAN_FOR_MODULES would have it scanned as one.
		CXX_SCAN_FOR_MODULES
			OFF
)
# --- tinyobjloader setup --

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

# --- ImGui setup --
# Two repositories pinned together: ImGui itself, and the C++ module binding for it, which is a
# generated `export using ::X;` skin over imgui.h rather than a library of its own. Neither ships a
# usable CMakeLists (the binding ships none at all), so SOURCE_SUBDIR names a directory that does not
# exist and the targets are written here. The binding is generated against one ImGui release, so the
# two tags move together; it covers the whole public ImGui:: surface but no macros, since those
# cannot cross a module boundary. GIT_SUBMODULES "" keeps the binding's cimgui submodule, which is
# only its generator's input, out of the checkout.
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b-docking SOURCE_SUBDIR does-not-exist)

FetchContent_Declare(
	imguimodule
	GIT_REPOSITORY https://github.com/stripe2933/imgui-module.git
	GIT_TAG 49e9e298293e2c89e881c1871b864a22a63e204d
	GIT_SUBMODULES
		""
	SOURCE_SUBDIR does-not-exist
)

FetchContent_MakeAvailable(
	imgui
	imguimodule
)

# ImGui's own sources. Split from the module binding below because a target carrying a CXX_MODULES
# file set has every one of its C++ sources scanned for modules, and these are plain TUs. Kept at
# C++26 because the global -freflection breaks any C++ TU compiled below it.
add_library(
	ImGuiLibrary
	STATIC
	"${imgui_SOURCE_DIR}/imgui.cpp"
	"${imgui_SOURCE_DIR}/imgui_draw.cpp"
	"${imgui_SOURCE_DIR}/imgui_tables.cpp"
	"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
	"${imgui_SOURCE_DIR}/imgui_demo.cpp"
	# The Vulkan backend is the one stock backend kept. Its platform counterpart is not: the engine
	# feeds ImGui from PlatformEventRecord instead, so ImGui never reaches the window layer.
	"${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

target_compile_features(ImGuiLibrary PUBLIC cxx_std_26)

target_include_directories(
	ImGuiLibrary
	SYSTEM
	PUBLIC
		"${imgui_SOURCE_DIR}"
		"${imgui_SOURCE_DIR}/backends"
)

# Vulkan::Vulkan rather than VulkanModule: the backend is C++ using the plain C headers, and it is
# the renderer that speaks vulkan-hpp.
target_link_libraries(ImGuiLibrary PUBLIC Vulkan::Vulkan)

target_compile_options(ImGuiLibrary PRIVATE -w)

# Carried on the ImGui target itself for the same reason GLM_FORCE_DEPTH_ZERO_TO_ONE is: they change
# the definitions of imgui.h's inline entities, so a TU disagreeing about them is an ODR violation.
# The module binding's own include of imgui.h is one such TU, and reaches them through the link
# below. DEFINE_MATH_OPERATORS is also what gates the binding's export of the ImVec2/ImVec4 operators.
target_compile_definitions(
	ImGuiLibrary
	PUBLIC
		IMGUI_DISABLE_OBSOLETE_FUNCTIONS
		IMGUI_DEFINE_MATH_OPERATORS
)

set_target_properties(
	ImGuiLibrary
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			OFF
		# Not module units, and the global CMAKE_CXX_SCAN_FOR_MODULES would have them scanned as
		# such. CXX_MODULE_STD OFF for the matching reason: a synthetic std module built with this
		# target's flags disagrees with the one every other target imports.
		CXX_SCAN_FOR_MODULES
			OFF
		CXX_MODULE_STD
			OFF
)

# The generated binding, given its own target the way VulkanModule is: third-party generated source
# that does not compile clean under the engine's warning set. The definitions above bake into the
# BMI, so importers need none of them.
add_library(ImGuiModule STATIC)

target_compile_features(ImGuiModule PUBLIC cxx_std_26)

target_link_libraries(ImGuiModule PUBLIC ImGuiLibrary)

target_compile_options(ImGuiModule PRIVATE -w)

set_target_properties(
	ImGuiModule
	PROPERTIES
		COMPILE_WARNING_AS_ERROR
			OFF
		# The binding imports nothing, so it needs no std module of its own.
		CXX_MODULE_STD
			OFF
)

target_sources(
	ImGuiModule
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				"${imguimodule_SOURCE_DIR}/generated"
			FILES
				"${imguimodule_SOURCE_DIR}/generated/imgui.cppm"
				# The settings handler that lets the debug UI store its own values in ImGui's ini
				# lives in ImGui's internal header, so its binding is built too.
				"${imguimodule_SOURCE_DIR}/generated/imgui_internal.cppm"
)
# --- ImGui setup --

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
	SOURCES
		${CMAKE_CURRENT_LIST_DIR}/src/Renderer/Shaders/shaders.slang
	ENTRY_POINTS
		vertMain
		fragMain
)

add_dependencies(PlaygroundEngine PlaygroundEngineShaders)
# --- Slang shader setup --

# --- Renderer texture setup --
# The renderer's own demo textures, kept beside its shaders because the renderer is what names them.
# Unlike the shaders there is no compile step, so the property carries the sources themselves; an
# executable target stages them next to its binary, since a static library has nowhere to put them.

# CONFIGURE_DEPENDS so dropping an image into Textures/ reaches the staging without editing this
# file. It re-globs on every build, which is affordable for a handful of demo assets and goes away
# with the asset system.

file(GLOB PGE_RENDERER_TEXTURES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/Renderer/Textures/*)

set_property(
	GLOBAL
	PROPERTY
		PlaygroundEngineTextures_SOURCE_FILES
			${PGE_RENDERER_TEXTURES}
)
# --- Renderer texture setup --

# --- Renderer model setup --
# The renderer's own demo meshes, published exactly the way the textures above are: globbed with
# CONFIGURE_DEPENDS so dropping an .obj into Models/ reaches the staging without editing this file,
# and carried on a global property because a static library has nowhere to stage them itself.

file(GLOB PGE_RENDERER_MODELS CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/Renderer/Models/*)

set_property(
	GLOBAL
	PROPERTY
		PlaygroundEngineModels_SOURCE_FILES
			${PGE_RENDERER_MODELS}
)
# --- Renderer model setup --

target_link_libraries(
	PlaygroundEngine
	PUBLIC
		spdlog::spdlog
	PRIVATE
		glfw
		glm::glm
		ImGuiModule
		StbImage
		TinyObjLoader
		VulkanModule
)

target_sources(
	PlaygroundEngine
	PRIVATE
		src/Log.cpp
		src/Diagnostics.cpp
		src/ContractHandler.cpp
		src/Engine.cpp
		src/App.cpp
		src/main.cpp
		src/Files.cpp
		src/Paths.cpp
		src/FrameCapture.cpp
		src/DebugUI/DebugUI.cpp
		src/DebugUI/DebugPanelDrawer.cpp
		src/DebugUI/DebugUIInputTranslation.cpp
		src/Ecs/Ecs.cpp
		src/Ecs/System.cpp
		src/Ecs/EntityDebugPanelSystem/EntityDebugPanelSystem.cpp
		src/Ecs/InputSystem/InputSystem.cpp
		src/Image/Image.cpp
		src/Mesh/Mesh.cpp
		src/AgentChannel/AgentChannel.cpp
		src/WindowServer/WindowServer.cpp
		src/WindowServer/Window.cpp
		src/WindowServer/Backend/WindowServerBackend.GLFW.cpp
		src/WindowServer/Backend/WindowBackend.GLFW.cpp
		src/WindowServer/Backend/GlfwInputTranslation.cpp
		src/Reflection/Core/TypedRef.cpp
		src/Reflection/Core/TypeInfo.cpp
		src/Reflection/Core/FieldInfo.cpp
		src/Reflection/Core/FunctionInfo.cpp
		src/Reflection/Core/Core.cpp
		src/Reflection/Builtins/EnumerationFacet.cpp
		src/MeshCatalog.cpp
		src/RenderExtraction.cpp
		src/Renderer/View.cpp
		src/Renderer/RendererVulkan.cpp
		src/Renderer/VulkanUtils.cpp
	PUBLIC
		FILE_SET CXX_MODULES
			BASE_DIRS
				src
			FILES
				src/Log.cppm
				src/Diagnostics.cppm
				src/Engine.cppm
				src/App.cppm
				src/Files.cppm
				src/Paths.cppm
				src/FrameCapture.cppm
				src/DebugUI/DebugUI.cppm
				src/DebugUI/DebugPanelDrawer.cppm
				src/DebugUI/DrawDebugAnnotation.cppm
				src/Ecs/CameraComponent.cppm
				src/Ecs/MeshComponent.cppm
				src/Ecs/Ecs.cppm
				src/Ecs/Entity.cppm
				src/Ecs/Component.cppm
				src/Ecs/TransformComponent.cppm
				src/Ecs/System.cppm
				src/Ecs/EntityDebugPanelSystem/EntityDebugPanelSystem.cppm
				src/Ecs/InputSystem/InputSystem.cppm
				src/Ecs/InputSystem/InputStateComponent.cppm
				src/Math/Math.cppm
				src/Math/Types.cppm
				src/Math/Transform.cppm
				src/Image/Image.cppm
				src/Mesh/Mesh.cppm
				src/PlatformEvents/PlatformEvents.cppm
				src/PlatformEvents/InputCode.cppm
				src/PlatformEvents/InputModifiers.cppm
				src/PlatformEvents/InputLockState.cppm
				src/PlatformEvents/PlatformKeyToken.cppm
				src/PlatformEvents/PlatformEvent.cppm
				src/PlatformEvents/PlatformEventRecord.cppm
				src/AgentChannel/AgentChannel.cppm
				src/WindowServer/WindowServer.cppm
				src/WindowServer/Window.cppm
				src/WindowServer/WindowSizes.cppm
				src/WindowServer/CursorShape.cppm
				src/WindowServer/WindowSpecification.cppm
				src/WindowServer/WindowServerErrors.cppm
				src/WindowServer/BackendDeclarations.cppm
				src/WindowServer/Backend/WindowServerBackend.GLFW.cppm
				src/WindowServer/Backend/WindowBackend.GLFW.cppm
				src/WindowServer/Backend/GlfwInputTranslation.cppm
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
				src/MeshCatalog.cppm
				src/RenderExtraction.cppm
				src/Renderer/Frame.cppm
				src/Renderer/Mesh.cppm
				src/Renderer/View.cppm
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
