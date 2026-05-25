#pragma once
#include <memory>

#include "spdlog/logger.h"

namespace PlaygroundEngine
{

    class Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return _logger; }
    private:
        static std::shared_ptr<spdlog::logger> _logger;
    };

}

#define LOG_TRACE(...)    ::PlaygroundEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::PlaygroundEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::PlaygroundEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::PlaygroundEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_FATAL(...)    ::PlaygroundEngine::Log::GetCoreLogger()->fatal(__VA_ARGS__)
