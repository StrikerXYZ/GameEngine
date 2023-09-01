#pragma once

#include "Definition.hpp"
#include "Core.hpp"

#include <spdlog/spdlog.h>

namespace spdlog
{
	class logger;
};

namespace Engine
{
	namespace Log
	{
		enum class Level
		{
            Trace,
            Debug,
			Info,
			Warn,
			Error,
			Critical
		};

		void Init();
		std::shared_ptr <spdlog::logger> GetCoreLogger();
		std::shared_ptr <spdlog::logger> GetClientLogger();

		template<typename T>
		void LogCore(Level level, const T& message)
		{
			switch (level)
			{
				default:
				case Engine::Log::Level::Trace:
					GetCoreLogger()->trace(message);
					return;
				case Engine::Log::Level::Debug:
					GetCoreLogger()->debug(message);
					break;
				case Engine::Log::Level::Info:
					GetCoreLogger()->info(message);
					break;
				case Engine::Log::Level::Warn:
					GetCoreLogger()->warn(message);
					break;
				case Engine::Log::Level::Error:
					GetCoreLogger()->error(message);
					break;
				case Engine::Log::Level::Critical:
					GetCoreLogger()->critical(message);
					break;
			}
		}

		template<typename T>
		void Log(Level level, const T& message)
		{
			switch (level)
			{
			default:
			case Engine::Log::Level::Trace:
				GetClientLogger()->trace(message);
				return;
			case Engine::Log::Level::Debug:
				GetClientLogger()->debug(message);
				break;
			case Engine::Log::Level::Info:
				GetClientLogger()->info(message);
				break;
			case Engine::Log::Level::Warn:
				GetClientLogger()->warn(message);
				break;
			case Engine::Log::Level::Error:
				GetClientLogger()->error(message);
				break;
			case Engine::Log::Level::Critical:
				GetClientLogger()->critical(message);
				break;
			}
		}

	}
};