#pragma once

#include <spdlog/spdlog.h>

#define LOG_TRACE(...)    ::PgE::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::PgE::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::PgE::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::PgE::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...)    ::PgE::Log::GetCoreLogger()->fatal(__VA_ARGS__)
