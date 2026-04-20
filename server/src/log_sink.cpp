#include "server/log_sink.h"

#include "server/async_logger.h"

#include <iostream>

namespace server {

FileSink::FileSink(const std::string& path)
    : file_(path, std::ios::app) {
    if (!file_.is_open()) {
        std::cerr << "Failed to open log file: " << path << "\n";
    }
}

void FileSink::write(LogLevel, const std::string& formatted) {
    file_ << formatted << "\n";
}

void FileSink::flush() {
    file_.flush();
}

void ConsoleSink::write(LogLevel level, const std::string& formatted) {
    auto& out = (level >= LogLevel::Warn) ? std::cerr : std::cout;
    out << formatted << "\n";
}

void ConsoleSink::flush() {
    std::cout.flush();
    std::cerr.flush();
}

} // namespace server
