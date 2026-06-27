module;

#include "PlaygroundEngine/Log.h"

export module PlaygroundEngine.Log;

import std;

namespace PgE
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
