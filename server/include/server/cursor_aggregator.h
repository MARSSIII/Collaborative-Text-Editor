#pragma once

#include "server/document_manager.h"
#include "server/async_logger.h"

#include <chrono>
#include <thread>

namespace server {

class CursorAggregator {
public:
    CursorAggregator(DocumentManager& doc_manager,
                     std::chrono::milliseconds interval,
                     AsyncLogger& logger);
    ~CursorAggregator();

    CursorAggregator(const CursorAggregator&) = delete;
    CursorAggregator& operator=(const CursorAggregator&) = delete;

    void start();
    void stop();

private:
    void broadcast_loop(std::stop_token stop);

    DocumentManager& doc_manager_;
    std::chrono::milliseconds interval_;
    AsyncLogger& logger_;
    std::jthread thread_;
};

}
