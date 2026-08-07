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

	std::expected<std::filesystem::path, PathError> GenerateCapturePath()
	{
		const std::expected<std::filesystem::path, PathError> executableDirectory = GetExecutableDirectory();
		if (!executableDirectory)
		{
			return std::unexpected(executableDirectory.error());
		}

		const std::filesystem::path capturesDirectory = *executableDirectory / "captures";

		// WriteBinaryFile does not create parents, so the directory has to exist before the capture
		// reaches it rather than after.

		std::error_code errorCode;
		std::filesystem::create_directories(capturesDirectory, errorCode);

		if (errorCode)
		{
			return std::unexpected(PathError::CaptureDirectoryUnavailable);
		}

		// The counter separates two captures landing in the same millisecond. Atomic because the
		// agent channel names captures on its reader thread while the key binding names them on the
		// main thread.

		static std::atomic<std::uint64_t> nextCaptureIndex{0};

		const std::chrono::time_point capturedAt = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());

		return capturesDirectory / std::format("capture-{:%Y%m%d-%H%M%S}-{}.png", capturedAt, nextCaptureIndex.fetch_add(1));
	}
}
