#pragma once

#include "collab/document.h"
#include "collab/access_control.h"
#include "server/document_session.h"
#include "server/async_logger.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace server {

class DocumentManager {
public:
    DocumentManager(collab::AccessControl& access,
                    const std::string& data_dir,
                    AsyncLogger& logger);

    uint32_t create_document(const std::string& title, uint32_t ownerId);
    std::shared_ptr<collab::Document> get_document(uint32_t docId);
    bool delete_document(uint32_t docId);

    struct DocListItem {
        uint32_t docId;
        std::string title;
        std::string role;
        int onlineCount;
    };
    std::vector<DocListItem> list_documents(uint32_t userId);

    std::vector<std::shared_ptr<collab::Document>> all_loaded_documents();

    std::shared_ptr<DocumentSession> get_or_create_session(uint32_t docId);
    std::shared_ptr<DocumentSession> get_session(uint32_t docId);
    void remove_session(uint32_t docId);

    void update_cursor(uint32_t docId, CursorState state);
    std::unordered_map<uint32_t, CursorState> get_cursors(uint32_t docId);
    void remove_cursor(uint32_t docId, uint32_t userId);

    std::vector<uint32_t> all_active_doc_ids();

    std::string role_to_string(uint32_t docId, uint32_t userId);

private:
    std::shared_ptr<collab::Document> load_from_disk_unlocked(uint32_t docId);
    void save_meta(uint32_t docId, const std::string& title);
    std::string load_meta(uint32_t docId);

    collab::AccessControl& access_;
    std::string data_dir_;
    AsyncLogger& logger_;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::shared_ptr<collab::Document>> documents_;
    std::unordered_map<uint32_t, std::string> titles_;
    std::unordered_map<uint32_t, std::shared_ptr<DocumentSession>> sessions_;

    std::unordered_map<uint32_t,
        std::unordered_map<uint32_t, CursorState>> cursor_buffers_;

    uint32_t next_doc_id_{1};
};

} // namespace server
