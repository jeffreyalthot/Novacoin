#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry {
    std::uint64_t timestamp = 0;
    LogLevel level = LogLevel::Info;
    std::string component;
    std::string message;
};

class Logger {
public:
    explicit Logger(std::size_t maxEntries = 500);

    void log(LogLevel level, const std::string& component, const std::string& message);
    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warning(const std::string& component, const std::string& message);
    void error(const std::string& component, const std::string& message);

    [[nodiscard]] std::vector<LogEntry> entries() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;

    static std::string format(const LogEntry& entry);

private:
    std::size_t maxEntries_ = 500;
    std::deque<LogEntry> entries_;

    static std::uint64_t nowSeconds();
};
