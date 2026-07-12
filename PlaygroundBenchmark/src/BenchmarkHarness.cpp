module PlaygroundBenchmark.Harness;

import std;

namespace PgE::Benchmark
{
	void Report(const Result& result)
	{
		std::println("{:<44} {:>12} iters {:>10.2f} ns/op", result.Name, result.Iterations, result.NanosecondsPerOperation);
	}

	void ReportRelative(const Result& baseline, const Result& measured)
	{
		const double overheadFactor =
			baseline.NanosecondsPerOperation == 0.0 ? 0.0 : measured.NanosecondsPerOperation / baseline.NanosecondsPerOperation;
		const double overheadNanoseconds = measured.NanosecondsPerOperation - baseline.NanosecondsPerOperation;

		std::println("{:<44} {:>10.2f}x {:>10.2f} ns overhead vs baseline", measured.Name, overheadFactor, overheadNanoseconds);
	}
}
