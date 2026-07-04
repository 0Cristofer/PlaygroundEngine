import std;

import PlaygroundEngine.Reflection;
import PlaygroundBenchmark.Harness;

namespace
{
	// Same shape as the reflection tests' Widget: a const, no-argument accessor and a
	// two-argument mutator, so both the return-boxing path and the argument-marshalling path
	// of the erased invoker are exercised. Mix is a deliberately heavier callee (a loop with
	// branching and nontrivial arithmetic) used to see how the dispatch overhead compares once
	// the reflected function itself does real work.
	struct Widget
	{
		int Width = 0;
		int Height = 0;

		int Area() const { return Width * Height; }

		void Resize(int width, int height)
		{
			Width = width;
			Height = height;
		}

		int Mix(int seed) const
		{
			// Mixing runs in unsigned arithmetic so the wraparound is well defined (signed
			// overflow would be UB and let the optimizer reshape the timed loop).
			std::uint32_t value = static_cast<std::uint32_t>(seed) ^ static_cast<std::uint32_t>(Width);
			for (int round = 0; round < 32; ++round)
			{
				value = value * 31u + static_cast<std::uint32_t>(Height);
				if ((value & 1u) != 0u)
					value ^= value >> 3;
				else
					value += static_cast<std::uint32_t>(Width);
			}
			return static_cast<int>(value);
		}
	};

	// Moderately complex, deterministic work standing in for realistic surrounding logic. Driven
	// by the runtime seed so it cannot be folded, and shared identically between the work-before-call
	// cases so the timing delta between them isolates the call mechanism.
	int SimulateWork(const int seed)
	{
		// Unsigned arithmetic so the LCG wraparound is well defined rather than signed-overflow UB.
		std::uint32_t value = static_cast<std::uint32_t>(seed);
		for (int step = 0; step < 64; ++step)
			value = value * 1103515245u + 12345u;

		return static_cast<int>(value);
	}

	std::size_t ParseArgument(const int argc, char** argv, const int index, const std::size_t fallback)
	{
		if (index >= argc)
			return fallback;

		const std::string_view text = argv[index];
		std::size_t value = fallback;
		const auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc{} || pointer != text.data() + text.size())
			return fallback;

		return value;
	}
}

int main(int argc, char** argv)
{
	using namespace PgE;
	using namespace PgE::Benchmark;

	// Iteration count and the starting dimension come from the command line so the benchmark
	// inputs are runtime-unknown: the compiler cannot fold the call bodies to constants. Both
	// have sensible defaults when absent.
	const std::size_t iterations = ParseArgument(argc, argv, 1, 2'000'000);
	const int startingDimension = static_cast<int>(ParseArgument(argc, argv, 2, 3));

	Widget widget{.Width = startingDimension, .Height = startingDimension + 1};

	const FuncInfo* area = TypeOf<Widget>().FindFunctionsByIdentifier("Area").front();
	const FuncInfo* resize = TypeOf<Widget>().FindFunctionsByIdentifier("Resize").front();
	const FuncInfo* mix = TypeOf<Widget>().FindFunctionsByIdentifier("Mix").front();

	// Every loop body advances a `dimension` that is barriered so the optimizer must treat it as
	// freshly changed each iteration, feeds it into the call, and accumulates the result. The
	// direct and reflected loops share identical surrounding work, so the timing difference is
	// the call mechanism itself and neither loop can be hoisted or folded to a constant.

	long long accumulator = 0;

	int areaDirectDimension = startingDimension;
	const Result areaDirect = Run("Area: direct native call", iterations, [&]
	{
		++areaDirectDimension;
		DoNotOptimize(areaDirectDimension);
		widget.Width = areaDirectDimension;
		accumulator += widget.Area();
		DoNotOptimize(accumulator);
	});

	int areaReflectedDimension = startingDimension;
	const Result areaReflected = Run("Area: reflected InvokeAs (cached lookup)", iterations, [&]
	{
		++areaReflectedDimension;
		DoNotOptimize(areaReflectedDimension);
		widget.Width = areaReflectedDimension;
		accumulator += area->InvokeAs<int>(&widget).value();
		DoNotOptimize(accumulator);
	});

	int resizeDirectDimension = startingDimension;
	const Result resizeDirect = Run("Resize: direct native call", iterations, [&]
	{
		++resizeDirectDimension;
		DoNotOptimize(resizeDirectDimension);
		widget.Resize(resizeDirectDimension, resizeDirectDimension + 1);
		DoNotOptimize(widget);
	});

	int resizeReflectedDimension = startingDimension;
	const Result resizeReflected = Run("Resize: reflected InvokeAs (cached lookup)", iterations, [&]
	{
		++resizeReflectedDimension;
		DoNotOptimize(resizeReflectedDimension);
		auto result = resize->InvokeAs(&widget, resizeReflectedDimension, resizeReflectedDimension + 1);
		DoNotOptimize(result);
		DoNotOptimize(widget);
	});

	int mixDirectDimension = startingDimension;
	const Result mixDirect = Run("Mix (heavy callee): direct native call", iterations, [&]
	{
		++mixDirectDimension;
		DoNotOptimize(mixDirectDimension);
		accumulator += widget.Mix(mixDirectDimension);
		DoNotOptimize(accumulator);
	});

	int mixReflectedDimension = startingDimension;
	const Result mixReflected = Run("Mix (heavy callee): reflected InvokeAs (cached lookup)", iterations, [&]
	{
		++mixReflectedDimension;
		DoNotOptimize(mixReflectedDimension);
		accumulator += mix->InvokeAs<int>(&widget, mixReflectedDimension).value();
		DoNotOptimize(accumulator);
	});

	// The lower-level erased path a visual-scripting VM would actually take: arguments and the
	// return slot are assembled into TypedRefs at runtime from generic slots, so the type tags and
	// marshalling are real work rather than the compile-time-folded tags InvokeAs<T> produces. The
	// int type tag is fetched once, as a VM would cache it per value type.
	const TypeInfo* const intTag = &TypeOf<int>();

	int areaErasedDimension = startingDimension;
	const Result areaErased = Run("Area: erased Invoke (runtime-assembled args)", iterations, [&]
	{
		++areaErasedDimension;
		DoNotOptimize(areaErasedDimension);
		widget.Width = areaErasedDimension;

		int areaResult = 0;
		auto result = area->Invoke(&widget, {}, TypedRef{.Type = intTag, .Data = &areaResult, .IsConst = false});
		DoNotOptimize(result);
		accumulator += areaResult;
		DoNotOptimize(accumulator);
	});

	int resizeErasedDimension = startingDimension;
	const Result resizeErased = Run("Resize: erased Invoke (runtime-assembled args)", iterations, [&]
	{
		++resizeErasedDimension;
		DoNotOptimize(resizeErasedDimension);

		int width = resizeErasedDimension;
		int height = resizeErasedDimension + 1;
		std::array<TypedRef, 2> args{
			TypedRef{.Type = intTag, .Data = &width, .IsConst = false},
			TypedRef{.Type = intTag, .Data = &height, .IsConst = false}
		};
		auto result = resize->Invoke(&widget, args);
		DoNotOptimize(result);
		DoNotOptimize(widget);
	});

	int mixErasedDimension = startingDimension;
	const Result mixErased = Run("Mix (heavy callee): erased Invoke (runtime-assembled args)", iterations, [&]
	{
		++mixErasedDimension;
		DoNotOptimize(mixErasedDimension);

		int seed = mixErasedDimension;
		std::array<TypedRef, 1> args{TypedRef{.Type = intTag, .Data = &seed, .IsConst = false}};
		int mixResult = 0;
		auto result = mix->Invoke(&widget, args, TypedRef{.Type = intTag, .Data = &mixResult, .IsConst = false});
		DoNotOptimize(result);
		accumulator += mixResult;
		DoNotOptimize(accumulator);
	});

	int workDirectDimension = startingDimension;
	const Result workThenDirect = Run("Work + call: work + direct native call", iterations, [&]
	{
		++workDirectDimension;
		DoNotOptimize(workDirectDimension);
		int worked = SimulateWork(workDirectDimension);
		DoNotOptimize(worked);
		widget.Width = worked;
		accumulator += widget.Area();
		DoNotOptimize(accumulator);
	});

	int workReflectedDimension = startingDimension;
	const Result workThenReflected = Run("Work + call: work + reflected InvokeAs", iterations, [&]
	{
		++workReflectedDimension;
		DoNotOptimize(workReflectedDimension);
		int worked = SimulateWork(workReflectedDimension);
		DoNotOptimize(worked);
		widget.Width = worked;
		accumulator += area->InvokeAs<int>(&widget).value();
		DoNotOptimize(accumulator);
	});

	DoNotOptimize(accumulator);

	std::println("PlaygroundBenchmark: reflection type-erased function calls");
	std::println("iterations={}, startingDimension={}", iterations, startingDimension);
	std::println("");

	Report(areaDirect);
	Report(areaReflected);
	Report(areaErased);
	Report(resizeDirect);
	Report(resizeReflected);
	Report(resizeErased);
	Report(mixDirect);
	Report(mixReflected);
	Report(mixErased);
	Report(workThenDirect);
	Report(workThenReflected);

	std::println("");
	std::println("Overhead relative to the matching direct native call:");
	ReportRelative(areaDirect, areaReflected);
	ReportRelative(areaDirect, areaErased);
	ReportRelative(resizeDirect, resizeReflected);
	ReportRelative(resizeDirect, resizeErased);
	ReportRelative(mixDirect, mixReflected);
	ReportRelative(mixDirect, mixErased);

	std::println("");
	std::println("Erased Invoke vs static-typed InvokeAs (the runtime-marshalling cost a VM adds):");
	ReportRelative(areaReflected, areaErased);
	ReportRelative(resizeReflected, resizeErased);
	ReportRelative(mixReflected, mixErased);

	std::println("");
	std::println("Work + call: reflected dispatch as a share of total iteration cost:");
	ReportRelative(workThenDirect, workThenReflected);
	const double dispatchDelta =
		workThenReflected.NanosecondsPerOperation - workThenDirect.NanosecondsPerOperation;
	const double dispatchShare = workThenReflected.NanosecondsPerOperation == 0.0
		                             ? 0.0
		                             : 100.0 * dispatchDelta / workThenReflected.NanosecondsPerOperation;
	std::println("Reflected dispatch delta: {:.2f} ns/op ({:.1f}% of the work+reflected iteration)",
	             dispatchDelta, dispatchShare);

	return 0;
}
