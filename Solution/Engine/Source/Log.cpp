#include "Log.hpp"
/*
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Engine
{
	namespace Log
	{
		global_static std::shared_ptr<spdlog::logger> core_logger;
		global_static std::shared_ptr<spdlog::logger> client_logger;
	}
}

void Engine::Log::Init()
{
	spdlog::set_pattern("%^[%T] %n:%v%$");

	Engine::Log::core_logger = spdlog::stdout_color_mt("Engine");
	Engine::Log::core_logger->set_level(spdlog::level::trace);

	Engine::Log::client_logger = spdlog::stdout_color_mt("App");
	Engine::Log::client_logger->set_level(spdlog::level::trace);
}

std::shared_ptr<spdlog::logger> Engine::Log::GetCoreLogger()
{
	return Engine::Log::core_logger;
}

std::shared_ptr<spdlog::logger> Engine::Log::GetClientLogger()
{
	return Engine::Log::client_logger;
}*/
