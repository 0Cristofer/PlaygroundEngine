module;

#include "PlaygroundEngine/Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

module PlaygroundEngine.Log;

namespace PlaygroundEngine
{
	std::shared_ptr<spdlog::logger> Log::_logger;

	void Log::Init()
	{
		spdlog::set_pattern("%^[%T] %n: %v%$");
		_logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("PlaygroundEngine");
		_logger->set_level(spdlog::level::trace);

		LOG_INFO("Logging initialized");
	}
}
