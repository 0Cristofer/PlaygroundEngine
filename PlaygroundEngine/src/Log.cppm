module;

#include "PlaygroundEngine/Log.h"

export module PlaygroundEngine.Log;

import std;

namespace PgE
{
	export enum class LogLevel
	{
		Trace = 0,
		Debug,
		Info,
		Warn,
		Error,
		Fatal,
		Off,
	};

	// Threshold for PGE_LOG's compile-time strip.
	export constexpr auto LOG_LEVEL_THRESHOLD =
#ifdef PGE_RELEASE
		LogLevel::Info;
#else
		LogLevel::Trace;
#endif

	namespace detail
	{
		// Exported (despite being detail) so importer-side Print instantiations link across module boundaries.

		export void LogDispatch(LogLevel level, const std::source_location& location, std::string_view message);
		export std::string ExtractQualifiedName(std::string_view signature);
	}

	export constexpr std::string_view LOG_FILE_PREFIX = "PlaygroundEngine";

	export struct LogConfiguration
	{
		// Empty selects the default, one timestamped file per run under the executable's log directory.
		std::filesystem::path FilePath;

		// Runs of log history kept on disk, this one included; the oldest go first.
		std::size_t MaximumFiles = 10;
	};

	export class Log
	{
	public:
		static void Configure(const LogConfiguration& configuration) pre(configuration.MaximumFiles > 0);

		// Writes out whatever is still buffered. Warnings and worse flush themselves;
		static void Flush();

		template <typename... Arguments>
		static void Print(const LogLevel level,
						  const std::source_location& location,
						  std::format_string<Arguments...> formatString,
						  Arguments&&... arguments)
		{
			// Formatting stays in-module so callers need no std::format import.
			detail::LogDispatch(level, location, std::format(formatString, std::forward<Arguments>(arguments)...));
		}

		static void Print(const LogLevel level, const std::source_location& location, const std::string_view message)
		{
			// Overload resolution routes any single-argument message here (logged
			// verbatim, not as a format string); the format template wins only with args.
			detail::LogDispatch(level, location, message);
		}

		static void Print(const LogLevel level, const std::source_location& location)
		{
			detail::LogDispatch(level, location, std::string_view{});
		}
	};
}
