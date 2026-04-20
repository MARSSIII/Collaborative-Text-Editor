#pragma once

#include "collab/mpsc_queue.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace server {

enum class LogLevel { Debug, Info, Warn, Error };

class ILogSink;

class AsyncLogger {
public:
    AsyncLogger(std::vector<std::unique_ptr<ILogSink>> sinks,
                LogLevel min_level = LogLevel::Info);
    ~AsyncLogger();

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);

    void shutdown();

private:
    void writer_loop(std::stop_token stop);
    static std::string level_str(LogLevel level);

    collab::MPSCQueue<std::pair<LogLevel, std::string>> queue_;
    std::jthread writer_thread_;
    std::vector<std::unique_ptr<ILogSink>> sinks_;
    LogLevel min_level_;
};

}
