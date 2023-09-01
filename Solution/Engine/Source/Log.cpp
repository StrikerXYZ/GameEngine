#include "Log.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Engine
{
	namespace Log
	{
		global_static std::shared_ptr<spdlog::logger> coreLogger;
		global_static std::shared_ptr<spdlog::logger> clientLogger;
	}
}

void Engine::Log::Init()
{
	spdlog::set_pattern("%^[%T] %n:%v%$");

	Engine::Log::coreLogger = spdlog::stdout_color_mt("Engine");
	Engine::Log::coreLogger->set_level(spdlog::level::trace);

	Engine::Log::clientLogger = spdlog::stdout_color_mt("App");
	Engine::Log::clientLogger->set_level(spdlog::level::trace);
}

std::shared_ptr<spdlog::logger> Engine::Log::GetCoreLogger()
{
	return Engine::Log::coreLogger;
}

std::shared_ptr<spdlog::logger> Engine::Log::GetClientLogger()
{
	return Engine::Log::clientLogger;
}