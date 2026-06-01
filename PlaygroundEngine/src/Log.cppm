module;

#include "PlaygroundEngine/Core.h"

export module PlaygroundEngine.Log;

import std;

namespace PlaygroundEngine
{
    export class Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return _logger; }
    private:
        static std::shared_ptr<spdlog::logger> _logger;
    };

}
