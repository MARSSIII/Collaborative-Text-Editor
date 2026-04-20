#include "server/async_logger.h"

#include "server/log_sink.h"

#include <chrono>
#include <format>

namespace server {

AsyncLogger::AsyncLogger(std::vector<std::unique_ptr<ILogSink>> sinks,
                         LogLevel min_level)
    : sinks_(std::move(sinks))
    , min_level_(min_level) {
    writer_thread_ = std::jthread([this](std::stop_token stop) {
        writer_loop(stop);
    });
}

AsyncLogger::~AsyncLogger() {
    shutdown();
}

void AsyncLogger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    auto entry = std::format("[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}] [{}] {}",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()),
        level_str(level), message);

    queue_.enqueue({level, std::move(entry)});
}

void AsyncLogger::info(const std::string& message) { log(LogLevel::Info, message); }
void AsyncLogger::warn(const std::string& message) { log(LogLevel::Warn, message); }
void AsyncLogger::error(const std::string& message) { log(LogLevel::Error, message); }
void AsyncLogger::debug(const std::string& message) { log(LogLevel::Debug, message); }

void AsyncLogger::shutdown() {
    if (writer_thread_.joinable()) {
        writer_thread_.request_stop();
        writer_thread_.join();
    }
}

void AsyncLogger::writer_loop(std::stop_token stop) {
    auto dispatch = [this](const std::pair<LogLevel, std::string>& item) {
        for (auto& sink : sinks_) sink->write(item.first, item.second);
    };

    while (!stop.stop_requested()) {
        bool wrote = false;
        while (auto entry = queue_.try_dequeue()) {
            dispatch(*entry);
            wrote = true;
        }
        if (wrote) {
            for (auto& sink : sinks_) sink->flush();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    while (auto entry = queue_.try_dequeue()) dispatch(*entry);
    for (auto& sink : sinks_) sink->flush();
}

std::string AsyncLogger::level_str(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "???";
}

} // namespace server
