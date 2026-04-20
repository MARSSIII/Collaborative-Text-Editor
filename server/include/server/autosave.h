#pragma once

#include "server/document_manager.h"
#include "server/async_logger.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace server {

class AutosaveThread {
public:
    AutosaveThread(DocumentManager& doc_manager,
                   const std::string& data_dir,
                   std::chrono::seconds interval,
                   uint32_t snapshot_threshold,
                   AsyncLogger& logger);
    ~AutosaveThread();

    AutosaveThread(const AutosaveThread&) = delete;
    AutosaveThread& operator=(const AutosaveThread&) = delete;

    void start();
    void stop();
    void force_save_all();

private:
    void run(std::stop_token stop);
    void save_document(collab::Document& doc, uint32_t docId);
    void maybe_create_snapshot(collab::Document& doc, uint32_t docId);

    DocumentManager& doc_manager_;
    std::string data_dir_;
    std::chrono::seconds interval_;
    AsyncLogger& logger_;

    std::jthread thread_;
    std::mutex cv_mutex_;
    std::condition_variable_any cv_;
};

}
