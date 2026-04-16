#pragma once

#include "collab/mpsc_queue.h"

#include <fstream>
#include <string>
#include <thread>

namespace server {

enum class LogLevel { Debug, Info, Warn, Error };

class AsyncLogger {
public:
    explicit AsyncLogger(const std::string& file_path,
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

    collab::MPSCQueue<std::string> queue_;
    std::jthread writer_thread_;
    std::ofstream file_;
    LogLevel min_level_;
};

} // namespace server
