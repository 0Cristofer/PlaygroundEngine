#pragma once

#include <source_location>

// Empty in shipping: source_location embeds full signatures, bloating the binary
// and leaking source paths.
#ifdef PGE_DEV
	#define PGE_LOG_LOCATION std::source_location::current()
#else
	#define PGE_LOG_LOCATION std::source_location{}
#endif

// `level` is a bare LogLevel enumerator; the if constexpr strips disabled levels at
// compile time, evaluating no arguments.
#define PGE_LOG(level, ...) \
	do { \
		if constexpr (::PgE::LogLevel::level >= ::PgE::LOG_LEVEL_THRESHOLD) \
			::PgE::Log::Print(::PgE::LogLevel::level, PGE_LOG_LOCATION __VA_OPT__(,) __VA_ARGS__); \
	} while (false)
