cmake_minimum_required(VERSION 4.3)

# --- doctest setup ---
include(FetchContent)

FetchContent_Declare(
		doctest
		GIT_REPOSITORY https://github.com/doctest/doctest.git
		GIT_TAG v2.5.2
		SYSTEM
		FIND_PACKAGE_ARGS
		CONFIG
		NAMES doctest
)

# We provide our own entry point (DOCTEST_CONFIG_IMPLEMENT + main() in TestMain.cpp),
# so doctest's bundled static-main lib is unwanted; don't build it.
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)

# doctest's CMakeLists declares cmake_minimum_required below CMake 4.x's supported
# floor; this lets its sub-configure proceed without editing the dep.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(doctest)
unset(CMAKE_POLICY_VERSION_MINIMUM)
# --- doctest setup ---

add_executable(PlaygroundTests)

target_compile_features(PlaygroundTests PRIVATE cxx_std_26)

target_compile_options(PlaygroundTests
		PRIVATE
		# Same warning bar as the rest of the project. doctest's headers are pulled in
		# SYSTEM (see FetchContent SYSTEM), so its own warnings are suppressed; only our
		# test code is held to this. -freflection comes from the global options (root).
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<CXX_COMPILER_ID:MSVC>:/permissive->

		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
)

set_target_properties(PlaygroundTests PROPERTIES
		COMPILE_WARNING_AS_ERROR ON
		CMAKE_LINK_WARNING_AS_ERROR ON
)

target_sources(PlaygroundTests
		PRIVATE
		src/TestMain.cpp
		src/Reflection/ReflectionTypeInfoTests.cpp
		src/Reflection/ReflectionStringificationTests.cpp
		src/Reflection/ReflectionFunctionTests.cpp
		src/Reflection/ReflectionFieldTests.cpp
		src/Reflection/ReflectionTypeGraphTests.cpp
		src/Reflection/ReflectionEnumTests.cpp
		src/Reflection/ReflectionAnnotationTests.cpp
		src/Reflection/ReflectionFacetTests.cpp
		src/LogTests.cpp
		src/SnapshotHarness.cpp
		src/SnapshotTests.cpp
		src/HarnessSelfVerificationTests.cpp
		src/ContractsTests.cpp
		src/ContractSeam.cpp
		src/RuntimeContractTests.cpp
		src/VerifyTests.cpp
		src/Scratch.cpp

		PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS src
		FILES
		src/Reflection/ReflectionTestTypes.cppm
		src/SnapshotHarness.cppm
		src/ContractSeam.cppm
)

# The snapshot harness resolves committed golden files relative to this directory at runtime.
target_compile_definitions(PlaygroundTests
		PRIVATE
		PGE_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/snapshots"
)

target_link_libraries(PlaygroundTests
		PRIVATE
		PlaygroundEngine
		doctest::doctest
)

# doctest ships a CTest integration helper that registers each TEST_CASE as an
# individual CTest entry (so `ctest -R <name>` runs a single test), and picks up
# new cases automatically without hand-maintained add_test() lists.
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(PlaygroundTests)
