export module PlaygroundEngine.Paths;

import std;

namespace PgE
{
	export enum class PathError
	{
		ExecutablePathUnavailable,
		CaptureDirectoryUnavailable,
	};

	// Absolute directory holding the running executable, resolved once per process. Runtime file
	// lookups compose against it with operator/ so they do not depend on the working directory the
	// process happened to be launched from.

	export std::expected<std::filesystem::path, PathError> GetExecutableDirectory();

	// Absolute path for the next frame capture, under the executable's captures directory, created if
	// absent. Names are unique across runs, not just within one: a reused name lets a caller polling
	// for its screenshot find an older run's file and read it as the current frame.

	export std::expected<std::filesystem::path, PathError> GenerateCapturePath();
}
