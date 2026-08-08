module;

#include "PlaygroundEngine/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

module PlaygroundEngine.Log;

import std;
import PlaygroundEngine.Paths;

namespace PgE
{
	namespace
	{
		std::shared_ptr<spdlog::logger> MakeDefaultLogger()
		{
			auto logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("PlaygroundEngine");

			// %-8l left-pads the level name to 8 (spdlog's longest, "critical") so the
			// field is fixed-width. %! renders the source_loc function name, which carries our
			// parsed namespace::class::function. Release omits the location field entirely.
#ifdef PGE_DEV
			logger->set_pattern("%^[%T] [%-8l] [%!] %v%$");
#else
			logger->set_pattern("%^[%T] [%-8l] %v%$");
#endif
			logger->set_level(spdlog::level::trace);
			logger->flush_on(spdlog::level::warn);

			return logger;
		}

		// Ambient L0 facility (see ApplicationArchitecture.md): the logger is created on first use with a
		// default stdout sink, so it is valid before Boot and after Shutdown, with no Engine. Configure() only
		// swaps sinks and levels on this same instance; function-local static init is thread-safe.
		std::shared_ptr<spdlog::logger>& DefaultLogger()
		{
			static std::shared_ptr<spdlog::logger> logger = MakeDefaultLogger();
			return logger;
		}

		// Null when the file cannot be opened, which is the only way this fails.
		std::shared_ptr<spdlog::sinks::sink> MakeFileSink(const std::filesystem::path& path)
		{
			// The sink's constructor reports a file it cannot open by throwing, and native runtime code must
			// not depend on unwinding, so the filesystem is asked first and every failure a caller can
			// cause is answered as an error instead.

			// Not covered is another process pulling the directory away between this probe and the
			// sink's own open, which still throws.

			if (const std::filesystem::path parentDirectory = path.parent_path(); !parentDirectory.empty())
			{
				std::error_code errorCode;
				std::filesystem::create_directories(parentDirectory, errorCode);

				if (errorCode)
				{
					return nullptr;
				}
			}

			if (const std::ofstream probe(path, std::ios::out | std::ios::trunc); !probe)
			{
				return nullptr;
			}

			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);

			// Set on the sink rather than the logger, which would overwrite the console sink's pattern. A
			// file line carries the date the console line omits, and no color range markers.

#ifdef PGE_DEV
			constexpr std::string_view filePattern = "[%Y-%m-%d %T.%eZ] [%-8l] [%!] %v";
#else
			constexpr std::string_view filePattern = "[%Y-%m-%d %T.%eZ] [%-8l] %v";
#endif

			fileSink->set_formatter(std::make_unique<spdlog::pattern_formatter>(std::string(filePattern), spdlog::pattern_time_type::utc));

			return fileSink;
		}

		constexpr std::string_view LOG_DIRECTORY_NAME = "logs";

		std::filesystem::path GenerateLogFileName()
		{
			const std::chrono::system_clock::time_point startedAt = std::chrono::system_clock::now();
			const std::chrono::time_point startedAtSecond = std::chrono::floor<std::chrono::seconds>(startedAt);
			const std::chrono::milliseconds millisecondsIntoSecond =
				std::chrono::duration_cast<std::chrono::milliseconds>(startedAt - startedAtSecond);

			return std::format("{}-{:%Y%m%d-%H%M%S}-{:03}.log", LOG_FILE_PREFIX, startedAtSecond, millisecondsIntoSecond.count());
		}

		std::filesystem::path ResolveLogPath(const LogConfiguration& configuration)
		{
			if (!configuration.FilePath.empty())
			{
				return configuration.FilePath;
			}

			const std::expected<std::filesystem::path, PathError> executableDirectory = GetExecutableDirectory();

			return executableDirectory ? *executableDirectory / LOG_DIRECTORY_NAME / GenerateLogFileName() : std::filesystem::path{};
		}

		void PruneLogHistory(const std::filesystem::path& logDirectory, const std::size_t maximumFilesToKeep)
		{
			// Every filesystem call takes an error_code, including the iterator's increment, which a
			// range-for would reach through its throwing overload. This runs during boot and a log directory
			// that cannot be read is no reason to fail one, so a failure here just prunes nothing.

			const std::string logFileNamePrefix = std::format("{}-", LOG_FILE_PREFIX);

			std::error_code errorCode;
			std::filesystem::directory_iterator entry(logDirectory, errorCode);
			std::vector<std::filesystem::path> logFiles;

			for (const std::filesystem::directory_iterator directoryEnd; !errorCode && entry != directoryEnd; entry.increment(errorCode))
			{
				const std::filesystem::path fileName = entry->path().filename();

				if (std::error_code entryErrorCode;
					!entry->is_regular_file(entryErrorCode) || !fileName.native().starts_with(logFileNamePrefix) || fileName.extension() != ".log")
				{
					continue;
				}

				logFiles.push_back(entry->path());
			}

			if (errorCode || logFiles.size() <= maximumFilesToKeep)
			{
				return;
			}

			std::ranges::sort(logFiles);

			for (const std::filesystem::path& logFile : std::span(logFiles).first(logFiles.size() - maximumFilesToKeep))
			{
				std::error_code removeErrorCode;
				std::filesystem::remove(logFile, removeErrorCode);
			}
		}

		constexpr spdlog::level ToSpdlogLevel(const LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace:
				return spdlog::level::trace;
			case LogLevel::Debug:
				return spdlog::level::debug;
			case LogLevel::Info:
				return spdlog::level::info;
			case LogLevel::Warn:
				return spdlog::level::warn;
			case LogLevel::Error:
				return spdlog::level::err;
			case LogLevel::Fatal:
				return spdlog::level::critical;
			case LogLevel::Off:
				return spdlog::level::off;
			}
			return spdlog::level::off;
		}
	}

	namespace detail
	{
		std::string ExtractQualifiedName(const std::string_view signature)
		{
			// COMPILER-SPECIFIC: source_location::function_name() is implementation-defined.
			// Verified to be the full "<ret> ns::cls::fn(params)" signature on both GCC and the Clang reflection branch

			// Find the parameter list '(' at angle-bracket depth 0.
			int angleDepth = 0;
			std::size_t parenthesisIndex = std::string_view::npos;
			for (std::size_t i = 0; i < signature.size(); ++i)
			{
				if (const char character = signature[i]; character == '<')
				{
					++angleDepth;
				}
				else if (character == '>')
				{
					if (angleDepth > 0)
					{
						--angleDepth;
					}
				}
				else if (character == '(' && angleDepth == 0)
				{
					parenthesisIndex = i;
					break;
				}
			}
			const std::string_view prefix = (parenthesisIndex == std::string_view::npos) ? signature : signature.substr(0, parenthesisIndex);

			// The qualified name starts after the last depth-0 space (separating
			// it from the return type). No space => no return type (constructors).
			angleDepth = 0;
			std::size_t nameStart = 0;
			for (std::size_t i = 0; i < prefix.size(); ++i)
			{
				if (const char character = prefix[i]; character == '<')
				{
					++angleDepth;
				}
				else if (character == '>')
				{
					if (angleDepth > 0)
					{
						--angleDepth;
					}
				}
				else if (character == ' ' && angleDepth == 0)
				{
					nameStart = i + 1;
				}
			}
			std::string_view qualifiedName = prefix.substr(nameStart);
			while (!qualifiedName.empty() && (qualifiedName.front() == '*' || qualifiedName.front() == '&'))
			{
				qualifiedName.remove_prefix(1);
			}

			// Copy out, skipping any "@Module.Name" run (terminated by '::' / '<' / '(').
			std::string result;
			result.reserve(qualifiedName.size());
			bool skipping = false;
			for (const char character : qualifiedName)
			{
				if (character == '@')
				{
					skipping = true;
					continue;
				}
				if (skipping)
				{
					if (character == ':' || character == '<' || character == '(' || character == ' ')
					{
						skipping = false;
					}
					else
					{
						continue;
					}
				}
				result.push_back(character);
			}
			return result;
		}
	}

	namespace
	{
		// Interns each call site's parsed name so the pointer handed to spdlog's non-owning source_loc lives
		// for the program's duration, safe for any sink. Keyed on the function_name() pointer, a stable
		// per-call-site static, so a given site is parsed only once.
		const char* InternQualifiedName(const char* signature)
		{
			static std::mutex poolMutex;
			static std::unordered_map<const char*, std::string> pool;

			const std::scoped_lock lock(poolMutex);
			auto [entry, inserted] = pool.try_emplace(signature);
			if (inserted)
			{
				entry->second = detail::ExtractQualifiedName(signature);
			}
			return entry->second.c_str();
		}
	}

	void Log::Configure(const LogConfiguration& configuration)
	{
		const std::filesystem::path logPath = ResolveLogPath(configuration);

		if (logPath.empty())
		{
			PGE_LOG(Warn, "File logging disabled: the log directory could not be resolved");
			return;
		}

		PruneLogHistory(logPath.parent_path(), configuration.MaximumFiles - 1);

		// Built before the installed sink is taken out, so a path that cannot be opened leaves the file
		// logging that was already working in place instead of replacing it with nothing.

		std::shared_ptr<spdlog::sinks::sink> fileSink = MakeFileSink(logPath);

		if (!fileSink)
		{
			PGE_LOG(Warn, "File logging unchanged: {} could not be opened", logPath.display_string());
			return;
		}

		// MakeDefaultLogger creates the logger holding the console sink and nothing else ever inserts, so
		// sink 0 is the console and whatever follows it is the log file this call replaces.

		std::vector<spdlog::sink_ptr>& sinks = DefaultLogger()->sinks();
		sinks.resize(1);
		sinks.push_back(std::move(fileSink));

		// After the sink is installed, so it is also the log file's own first line.

		PGE_LOG(Info, "Logging to {}", logPath.display_string());
	}

	void Log::Flush()
	{
		DefaultLogger()->flush();
	}

	namespace detail
	{
		void LogDispatch(const LogLevel level, const std::source_location& location, const std::string_view message)
		{
			const spdlog::source_loc spdlogLocation{location.file_name(), location.line(), InternQualifiedName(location.function_name())};

			DefaultLogger()->log(spdlogLocation, ToSpdlogLevel(level), message);
		}
	}
}
