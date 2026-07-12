export module PlaygroundBenchmark.Harness;

import std;

// A deliberately small timing harness: warm up, then time a fixed number of iterations of a callable and
// report nanoseconds per operation. No external benchmark framework is pulled in because global -freflection
// leaks onto FetchContent deps compiled below C++26 and breaks them; a hand-rolled loop sidesteps that.

namespace PgE::Benchmark
{
	export template <typename T>
	void DoNotOptimize(T& value)
	{
		// Forces the value to be treated as read and clobbered, so the timed body cannot be
		// optimized away or hoisted out of the loop.
		__asm__ __volatile__("" : "+m,r"(value) : : "memory");
	}

	export struct Result
	{
		std::string_view Name;
		std::size_t Iterations = 0;
		double NanosecondsPerOperation = 0.0;
	};

	export void Report(const Result& result);

	export void ReportRelative(const Result& baseline, const Result& measured);

	export template <typename Callable>
	Result Run(const std::string_view name, const std::size_t iterations, Callable&& body)
	{
		constexpr std::size_t warmupIterations = 4096;
		for (std::size_t iteration = 0; iteration < warmupIterations; ++iteration)
		{
			body();
		}

		const auto start = std::chrono::steady_clock::now();
		for (std::size_t iteration = 0; iteration < iterations; ++iteration)
		{
			body();
		}
		const auto end = std::chrono::steady_clock::now();

		const auto elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

		// Guard the divide here rather than in the argument parser, so a zero iteration count
		// reports 0 ns/op instead of NaN while a legitimately zero numeric input elsewhere stays valid.
		return Result{.Name = name,
					  .Iterations = iterations,
					  .NanosecondsPerOperation = iterations == 0 ? 0.0 : static_cast<double>(elapsedNanoseconds) / static_cast<double>(iterations)};
	}
}
