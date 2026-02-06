#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
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

    void log(LogLevel level, std::string_view component, std::string_view message);
    void debug(std::string_view component, std::string_view message);
    void info(std::string_view component, std::string_view message);
    void warning(std::string_view component, std::string_view message);
    void error(std::string_view component, std::string_view message);

    [[nodiscard]] std::vector<LogEntry> entries() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;
    void clear();

    void setMinLevel(LogLevel level);
    [[nodiscard]] LogLevel minLevel() const;
    void setMaxEntries(std::size_t maxEntries);
    [[nodiscard]] std::size_t maxEntries() const;

    static std::string format(const LogEntry& entry);

private:
    std::size_t maxEntries_ = 500;
    std::deque<LogEntry> entries_;
    LogLevel minLevel_ = LogLevel::Debug;
    mutable std::mutex mutex_;

    static std::uint64_t nowSeconds();
};
