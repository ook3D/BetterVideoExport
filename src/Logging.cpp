#include "Logging.h"

namespace logging 
{
    void Info(const char* message)
    {
        spdlog::info(message);
    }

    void Warning(const char* message)
    {
        spdlog::warn(message);
    }

    void Error(const char* message)
    {
        spdlog::error(message);
    }

    void Initialize(const char* loggerName, const char* logFile)
    {
        {
            std::ofstream(logFile, std::ios::trunc).close();
        }

        std::shared_ptr<spdlog::logger> logger = spdlog::get(loggerName);
        if (!logger)
        {
            logger = spdlog::basic_logger_mt(loggerName, logFile);
        }

        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%H:%M:%S] %v");
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        spdlog::flush_every(std::chrono::seconds(2));
    }

    void LogStartupBanner(int gameBuild)
    {
        spdlog::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        spdlog::info("    ASI Build: {} {}", __DATE__, __TIME__);
        spdlog::info("    Game Build: {}", gameBuild);
        spdlog::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    }

    void LogSection(const char* title)
    {
        if (title)
        {
            spdlog::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            spdlog::info("  {}", title);
            spdlog::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        }
        else
        {
            spdlog::info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        }
    }

    void LogSubsection(const char* title)
    {
        spdlog::info("-- {}", title);
    }

    void LogInitialization(const char* component)
    {
        spdlog::info("Initializing {}", component);
    }

    void LogCompletion(const char* component, const char* details)
    {
        if (details)
        {
            spdlog::info("{} complete - {}", component, details);
        }
        else
        {
            spdlog::info("{} complete", component);
        }
    }

    void LogWithLocation(const char* file, int line, const char* message)
    {
        spdlog::info("{}:{} - {}", GetBasename(file), line, message);
    }

    void LogWithLocationF(const char* file, int line, const char* fmt)
    {
        std::string debugFmt = GetBasename(file) + std::string(":") + std::to_string(line) + " - " + fmt;
        spdlog::info(debugFmt);
    }
} // namespace logging
