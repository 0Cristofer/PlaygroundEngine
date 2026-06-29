module;

#include "PlaygroundEngine/Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

module PlaygroundEngine.Log;

import std;

namespace PgE
{
	namespace
	{
		std::shared_ptr<spdlog::logger> s_logger;

		constexpr spdlog::level ToSpdlogLevel(const LogLevel level)
		{
			switch (level)
			{
				case LogLevel::Trace: return spdlog::level::trace;
				case LogLevel::Debug: return spdlog::level::debug;
				case LogLevel::Info:  return spdlog::level::info;
				case LogLevel::Warn:  return spdlog::level::warn;
				case LogLevel::Error: return spdlog::level::err;
				case LogLevel::Fatal: return spdlog::level::critical;
				case LogLevel::Off:   return spdlog::level::off;
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
				if (const char character = signature[i]; character == '<') ++angleDepth;
				else if (character == '>') { if (angleDepth > 0) --angleDepth; }
				else if (character == '(' && angleDepth == 0) { parenthesisIndex = i; break; }
			}
			const std::string_view prefix =
				(parenthesisIndex == std::string_view::npos) ? signature : signature.substr(0, parenthesisIndex);

			// The qualified name starts after the last depth-0 space (separating
			// it from the return type). No space => no return type (constructors).
			angleDepth = 0;
			std::size_t nameStart = 0;
			for (std::size_t i = 0; i < prefix.size(); ++i)
			{
				if (const char character = prefix[i]; character == '<') ++angleDepth;
				else if (character == '>') { if (angleDepth > 0) --angleDepth; }
				else if (character == ' ' && angleDepth == 0) nameStart = i + 1;
			}
			std::string_view qualifiedName = prefix.substr(nameStart);
			while (!qualifiedName.empty() && (qualifiedName.front() == '*' || qualifiedName.front() == '&'))
				qualifiedName.remove_prefix(1);

			// Copy out, skipping any "@Module.Name" run (terminated by '::' / '<' / '(').
			std::string result;
			result.reserve(qualifiedName.size());
			bool skipping = false;
			for (const char character : qualifiedName)
			{
				if (character == '@') { skipping = true; continue; }
				if (skipping)
				{
					if (character == ':' || character == '<' || character == '(' || character == ' ') skipping = false;
					else continue;
				}
				result.push_back(character);
			}
			return result;
		}
	}

	namespace
	{
		// Interns each call site's parsed name so the pointer handed to spdlog's
		// non-owning source_loc lives for the program's duration — safe for any sink,
		// including a future asynchronous one. Keyed on the function_name() pointer,
		// a stable per-call-site static, so a given site is parsed only once.
		const char* InternQualifiedName(const char* signature)
		{
			static std::mutex poolMutex;
			static std::unordered_map<const char*, std::string> pool;

			const std::scoped_lock lock(poolMutex);
			auto [entry, inserted] = pool.try_emplace(signature);
			if (inserted)
				entry->second = detail::ExtractQualifiedName(signature);
			return entry->second.c_str();
		}
	}

	void Log::Init()
	{
		s_logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("PlaygroundEngine");

		// %-8l left-pads the level name to 8 (spdlog's longest, "critical") so the
		// field is fixed-width. %! renders the source_loc funcion name, which carries our
		// parsed namespace::class::function. Release omits the location field entirely.
#ifdef PGE_DEV
		s_logger->set_pattern("%^[%T] [%-8l] [%!] %v%$");
#else
		s_logger->set_pattern("%^[%T] [%-8l] %v%$");
#endif
		s_logger->set_level(spdlog::level::trace);

		PGE_LOG(Info, "Logging initialized");
	}

	namespace detail
	{
		void LogDispatch(const LogLevel level, const std::source_location& location, const std::string_view message)
		{
			// Exported (despite being detail) so importer-side Print instantiations link
			// across module boundaries.
			const spdlog::source_loc spdlogLocation{
				location.file_name(), location.line(), InternQualifiedName(location.function_name())};

			s_logger->log(spdlogLocation, ToSpdlogLevel(level), message);
		}
	}
}
