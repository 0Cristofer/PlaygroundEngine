cmake_minimum_required(VERSION 4.3)

add_executable(PlaygroundBenchmark)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
	message(FATAL_ERROR
			"[PlaygroundEngine] Benchmark requires GCC 16+.\n"
			"Current compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
	)
endif ()

target_compile_features(PlaygroundBenchmark PRIVATE cxx_std_26)

target_compile_definitions(PlaygroundBenchmark
		PRIVATE
		# Windows specifics to improve compile times
		$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
		$<$<PLATFORM_ID:Windows>:NOMINMAX>
		$<$<PLATFORM_ID:Windows>:UNICODE>
		$<$<PLATFORM_ID:Windows>:_UNICODE>
)

target_compile_options(PlaygroundBenchmark
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

set_target_properties(PlaygroundBenchmark PROPERTIES
		COMPILE_WARNING_AS_ERROR ON
		CMAKE_LINK_WARNING_AS_ERROR ON
		MSVC_RUNTIME_CHECKS "$<$<CONFIG:Debug>:UninitializedVariable;StackFrameErrorCheck>"
)

if (PGE_IPO_SUPPORTED)
	set_target_properties(PlaygroundBenchmark PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
	)
endif ()

target_sources(PlaygroundBenchmark
		PRIVATE
		src/main.cpp
		src/BenchmarkHarness.cpp

		PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS src
		FILES
		src/BenchmarkHarness.cppm
)

target_link_libraries(PlaygroundBenchmark
		PRIVATE
		PlaygroundEngine
)
