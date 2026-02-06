#include "utils/logger.hpp"

#include <algorithm>
#include <chrono>

namespace {
constexpr int levelValue(LogLevel level) {
    return static_cast<int>(level);
}

const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "INFO";
    }
}

bool shouldLog(LogLevel level, LogLevel minimum) {
    return levelValue(level) >= levelValue(minimum);
}
}

Logger::Logger(std::size_t maxEntries) : maxEntries_(std::max<std::size_t>(1, maxEntries)) {
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    if (!shouldLog(level, minLevel_)) {
        return;
    }

    if (entries_.size() == maxEntries_) {
        entries_.pop_front();
    }

    entries_.push_back(
        LogEntry{nowSeconds(), level, std::string(component), std::string(message)});
}

void Logger::debug(std::string_view component, std::string_view message) {
    log(LogLevel::Debug, component, message);
}

void Logger::info(std::string_view component, std::string_view message) {
    log(LogLevel::Info, component, message);
}

void Logger::warning(std::string_view component, std::string_view message) {
    log(LogLevel::Warning, component, message);
}

void Logger::error(std::string_view component, std::string_view message) {
    log(LogLevel::Error, component, message);
}

std::vector<LogEntry> Logger::entries() const {
    return std::vector<LogEntry>(entries_.begin(), entries_.end());
}

std::size_t Logger::size() const {
    return entries_.size();
}

bool Logger::empty() const {
    return entries_.empty();
}

void Logger::clear() {
    entries_.clear();
}

void Logger::setMinLevel(LogLevel level) {
    minLevel_ = level;
}

LogLevel Logger::minLevel() const {
    return minLevel_;
}

std::string Logger::format(const LogEntry& entry) {
    return "[" + std::to_string(entry.timestamp) + "] [" + toString(entry.level) + "] [" + entry.component +
           "] " + entry.message;
}

std::uint64_t Logger::nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
