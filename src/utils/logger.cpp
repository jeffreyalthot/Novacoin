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
            return "UNKNOWN";
    }
}

bool shouldLog(LogLevel level, LogLevel minimum) {
    return levelValue(level) >= levelValue(minimum);
}
}

Logger::Logger(std::size_t maxEntries) : maxEntries_(std::max<std::size_t>(1, maxEntries)) {
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!shouldLog(level, minLevel_)) {
        return;
    }

    if (entries_.size() >= maxEntries_) {
        entries_.pop_front();
    }

    entries_.emplace_back(
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
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<LogEntry> snapshot;
    snapshot.reserve(entries_.size());
    snapshot.assign(entries_.begin(), entries_.end());
    return snapshot;
}

std::size_t Logger::size() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.size();
}

bool Logger::empty() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return entries_.empty();
}

void Logger::clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    entries_.clear();
}

void Logger::setMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> guard(mutex_);
    minLevel_ = level;
}

LogLevel Logger::minLevel() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return minLevel_;
}

std::string Logger::format(const LogEntry& entry) {
    const std::string timestamp = std::to_string(entry.timestamp);
    const std::string level = toString(entry.level);
    std::string formatted;
    formatted.reserve(timestamp.size() + level.size() + entry.component.size() + entry.message.size() + 8);
    formatted.append("[");
    formatted.append(timestamp);
    formatted.append("] [");
    formatted.append(level);
    formatted.append("] [");
    formatted.append(entry.component);
    formatted.append("] ");
    formatted.append(entry.message);
    return formatted;
}

std::uint64_t Logger::nowSeconds() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}
