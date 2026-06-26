#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <fstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/fmt/bundled/format.h>

namespace logging 
{
    // Helper function to extract basename from file path
    constexpr const char* GetBasename(const char* path)
    {
        const char* file = path;
        while (*path) {
            if (*path == '/' || *path == '\\')
            {
                file = path + 1;
            }
            ++path;
        }
        return file;
    }
    
    void Info(const char* message);
    void Warning(const char* message);
    void Error(const char* message);

    template<typename... Args>
    void InfoF(const char* fmt, Args&&... args);

    template<typename... Args>
    void WarningF(const char* fmt, Args&&... args);

    template<typename... Args>
    void ErrorF(const char* fmt, Args&&... args);

    void Initialize(const char* loggerName, const char* logFile);
    void LogStartupBanner(int gameBuild);
    void LogSection(const char* title = nullptr);
    void LogSubsection(const char* title);
    void LogInitialization(const char* component);
    void LogCompletion(const char* component, const char* details = nullptr);
    void LogWithLocation(const char* file, int line, const char* message);
    void LogWithLocationF(const char* file, int line, const char* fmt);

    template<typename... Args>
    void LogWithLocationF(const char* file, int line, const char* fmt, Args&&... args);

    template<typename... Args>
    inline void InfoF(const char* fmt, Args&&... args)
    {
        spdlog::info(fmt::runtime(fmt), std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void WarningF(const char* fmt, Args&&... args)
    {
        spdlog::warn(fmt::runtime(fmt), std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void ErrorF(const char* fmt, Args&&... args)
    {
        spdlog::error(fmt::runtime(fmt), std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void LogWithLocationF(const char* file, int line, const char* fmt, Args&&... args)
    {
        std::string debugFmt = GetBasename(file) + std::string(":") + std::to_string(line) + " - " + fmt;
        spdlog::info(fmt::runtime(debugFmt), std::forward<Args>(args)...);
    }
} // namespace logging

// Convenience macros for backward compatibility and ease of use
#define logf(fmt, ...) logging::InfoF(fmt, __VA_ARGS__)

// Helper macro for logm - always use the formatted version which handles both cases
#define logm(...) logging::LogWithLocationF(__FILE__, __LINE__, __VA_ARGS__)

#define log_section(title) logging::LogSection(title)
#define log_subsection(title) logging::LogSubsection(title)
#define log_init(component) logging::LogInitialization(component)
#define log_complete(component, ...) logging::LogCompletion(component, ##__VA_ARGS__)
