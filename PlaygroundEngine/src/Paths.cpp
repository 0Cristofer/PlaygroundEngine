module PlaygroundEngine.Paths;

import std;

namespace PgE
{
	namespace
	{
		std::expected<std::filesystem::path, PathError> ResolveExecutableDirectory()
		{
			// TODO: /proc/self/exe is Linux-only. Windows wants GetModuleFileNameW and macOS
			// _NSGetExecutablePath, so this needs the same per-platform split the window backend has
			// once a second platform is actually supported.

			// canonical() reports failure by throwing unless handed an error_code, and native runtime
			// code must not depend on unwinding.

			std::error_code errorCode;
			const std::filesystem::path executablePath = std::filesystem::canonical("/proc/self/exe", errorCode);

			if (errorCode)
			{
				return std::unexpected(PathError::ExecutablePathUnavailable);
			}

			return executablePath.parent_path();
		}
	}

	std::expected<std::filesystem::path, PathError> GetExecutableDirectory()
	{
		// The executable cannot move out from under a running process, so the lookup happens once.

		static const std::expected<std::filesystem::path, PathError> executableDirectory = ResolveExecutableDirectory();

		return executableDirectory;
	}
}
