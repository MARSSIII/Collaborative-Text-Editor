#pragma once

#include <fstream>
#include <string>

namespace server {

enum class LogLevel;

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(LogLevel level, const std::string& formatted) = 0;
    virtual void flush() {}
};

class FileSink : public ILogSink {
public:
    explicit FileSink(const std::string& path);
    void write(LogLevel level, const std::string& formatted) override;
    void flush() override;

    bool is_open() const { return file_.is_open(); }

private:
    std::ofstream file_;
};

class ConsoleSink : public ILogSink {
public:
    void write(LogLevel level, const std::string& formatted) override;
    void flush() override;
};

}
