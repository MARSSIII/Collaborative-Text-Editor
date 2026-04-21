#include "server/autosave.h"

#include <filesystem>
#include <format>
#include <fstream>

namespace fs = std::filesystem;

namespace server {

AutosaveThread::AutosaveThread(DocumentManager& doc_manager,
                               const std::string& data_dir,
                               std::chrono::seconds interval,
                               uint32_t snapshot_threshold,
                               AsyncLogger& logger)
    : doc_manager_(doc_manager), data_dir_(data_dir)
    , interval_(interval)
    , logger_(logger) { (void)snapshot_threshold; }

AutosaveThread::~AutosaveThread() {
    stop();
}

void AutosaveThread::start() {
    thread_ = std::jthread([this](std::stop_token stop) { run(stop); });
}

void AutosaveThread::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        cv_.notify_all();
        thread_.join();
    }
}

void AutosaveThread::force_save_all() {
    auto docs = doc_manager_.all_loaded_documents();

    for (auto& doc : docs) {
        save_document(*doc, doc->id());
    }

    logger_.info("Force saved all documents");
}

void AutosaveThread::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        {
            std::unique_lock lock(cv_mutex_);
            cv_.wait_for(lock, stop, interval_,
                         [&] { return stop.stop_requested(); });
        }

        if (stop.stop_requested()) {
            return;
        }

        auto docs = doc_manager_.all_loaded_documents();
        for (auto& doc : docs) {
            if (doc->is_dirty()) {
                save_document(*doc, doc->id());
                maybe_create_snapshot(*doc, doc->id());
            }
        }
    }
}

void AutosaveThread::save_document(collab::Document& doc, uint32_t docId) {
    auto content = doc.get_content();
    auto revision = doc.revision();

    auto path = std::format("{}/documents/{}.txt", data_dir_, docId);
    auto tmp_path = path + ".tmp";

    std::ofstream file(tmp_path);
    if (!file.is_open()) {
        logger_.error(std::format("Failed to save document {}", docId));
        return;
    }

    file << content;
    file.close();

    fs::rename(tmp_path, path);
    doc.mark_saved();

    logger_.debug(std::format("Saved document {} (revision {})",
                              docId, revision));
}

void AutosaveThread::maybe_create_snapshot(collab::Document& doc,
                                           uint32_t docId) {
    if (!doc.needs_snapshot()) return;

    auto content = doc.get_content();
    auto revision = doc.revision();

    auto path = std::format("{}/snapshots/{}_{}.txt",
                            data_dir_, docId, revision);
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        doc.mark_snapshot_created();
        logger_.info(std::format("Created snapshot for doc {} at revision {}",
                                 docId, revision));
    }
}

}
