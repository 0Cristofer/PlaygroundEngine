export module PlaygroundEngine.Paths;

import std;

namespace PgE
{
	export enum class PathError
	{
		ExecutablePathUnavailable,
	};

	// Absolute directory holding the running executable, resolved once per process. Runtime file
	// lookups compose against it with operator/ so they do not depend on the working directory the
	// process happened to be launched from.
	export std::expected<std::filesystem::path, PathError> GetExecutableDirectory();
}
