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
		export void LogDispatch(LogLevel level, const std::source_location& location, std::string_view message);
		export std::string ExtractQualifiedName(std::string_view signature);
	}

	export class Log
	{
	public:
		static void Configure();

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

		static void Print(const LogLevel level, const std::source_location& location) { detail::LogDispatch(level, location, std::string_view{}); }
	};
}
