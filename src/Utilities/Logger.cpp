#include "Utilities/Logger.h"

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace rjo {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::wstring& logDirectory) {
    std::lock_guard lock(mutex_);
    logDirectory_ = logDirectory;
    std::error_code ec;
    fs::create_directories(logDirectory_, ec);
    initialized_ = true;
}

void Logger::Info(const std::string& message) {
    Write(LogLevel::Info, message);
}

void Logger::Warning(const std::string& message) {
    Write(LogLevel::Warning, message);
}

void Logger::Error(const std::string& message) {
    Write(LogLevel::Error, message);
}

void Logger::Write(LogLevel level, const std::string& message) {
    const char* tag = "INFO";
    switch (level) {
    case LogLevel::Warning: tag = "WARN"; break;
    case LogLevel::Error:   tag = "ERROR"; break;
    default: break;
    }

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
    localtime_s(&localTm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S")
        << " [" << tag << "] " << message;
    WriteLine(oss.str());
}

std::wstring Logger::BuildLogPath() const {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
    localtime_s(&localTm, &tt);

    wchar_t name[64]{};
    swprintf_s(name, L"retrojapan_%04d%02d%02d.log",
               localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday);
    return (fs::path(logDirectory_) / name).wstring();
}

void Logger::WriteLine(const std::string& line) {
    std::lock_guard lock(mutex_);
    if (!initialized_) {
        return;
    }

    std::ofstream out(BuildLogPath(), std::ios::app);
    if (out) {
        out << line << '\n';
    }

#ifdef _DEBUG
    OutputDebugStringA((line + "\n").c_str());
#endif
}

} // namespace rjo
