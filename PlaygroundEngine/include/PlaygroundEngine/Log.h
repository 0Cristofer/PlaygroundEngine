#pragma once

#include <spdlog/spdlog.h>

#define LOG_TRACE(...)    ::PlaygroundEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::PlaygroundEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::PlaygroundEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::PlaygroundEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...)    ::PlaygroundEngine::Log::GetCoreLogger()->fatal(__VA_ARGS__)
