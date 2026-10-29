#pragma once

#include <string>
#include <mutex>

namespace rjo {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& Instance();

    void Initialize(const std::wstring& logDirectory);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    void Write(LogLevel level, const std::string& message);

private:
    Logger() = default;
    std::wstring BuildLogPath() const;
    void WriteLine(const std::string& line);

    std::mutex mutex_;
    std::wstring logDirectory_;
    bool initialized_ = false;
};

} // namespace rjo
