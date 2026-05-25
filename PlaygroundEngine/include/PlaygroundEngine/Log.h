#pragma once
#include <memory>

//#include "vendor/spdlog/include/spdlog/logger.h"

namespace spdlog
{
    class logger
    {
    public:
        void trace(const char [], int = 0) {}
        void info(const char [], int = 0) {}
        void warn(const char [], int = 0) {}
        void error(const char [], int = 0) {}
        void fatal(const char [], int = 0) {}
    };
}

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
