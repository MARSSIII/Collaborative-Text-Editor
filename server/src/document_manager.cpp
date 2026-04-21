#include "server/document_manager.h"

#include <filesystem>
#include <format>
#include <fstream>

namespace fs = std::filesystem;

namespace server {

DocumentManager::DocumentManager(collab::AccessControl& access,
                                 const std::string& data_dir,
                                 AsyncLogger& logger)
    : access_(access), data_dir_(data_dir), logger_(logger) {
    fs::create_directories(data_dir_ + "/documents");
    fs::create_directories(data_dir_ + "/snapshots");
}

uint32_t DocumentManager::create_document(const std::string& title,
                                          uint32_t ownerId) {
    std::lock_guard lock(mutex_);
    uint32_t docId = next_doc_id_++;

    auto doc = std::make_shared<collab::Document>(docId, title);
    documents_[docId] = doc;
    titles_[docId] = title;

    access_.grant(docId, ownerId, collab::Role::Owner);

    auto path = std::format("{}/documents/{}.txt", data_dir_, docId);
    std::ofstream(path).close();
    save_meta(docId, title);

    return docId;
}

std::shared_ptr<collab::Document> DocumentManager::get_document(uint32_t docId) {
    {
        std::lock_guard lock(mutex_);
        auto it = documents_.find(docId);
        if (it != documents_.end()) return it->second;
    }

    auto loaded = load_from_disk_unlocked(docId);
    if (!loaded) return nullptr;

    std::lock_guard lock(mutex_);
    auto it = documents_.find(docId);
    if (it != documents_.end()) return it->second;

    documents_[docId] = loaded;
    titles_[docId] = loaded->title();

    if (docId >= next_doc_id_) next_doc_id_ = docId + 1;
    logger_.info(std::format("Loaded document {} from disk", docId));

    return loaded;
}

bool DocumentManager::delete_document(uint32_t docId) {
    std::lock_guard lock(mutex_);

    documents_.erase(docId);
    titles_.erase(docId);

    auto sit = sessions_.find(docId);
    if (sit != sessions_.end()) {
        sit->second->request_stop();
        sessions_.erase(sit);
    }

    cursor_buffers_.erase(docId);
    access_.revoke_all_for_document(docId);

    auto doc_path = std::format("{}/documents/{}.txt", data_dir_, docId);
    auto meta_path = std::format("{}/documents/{}.meta", data_dir_, docId);
    fs::remove(doc_path);
    fs::remove(meta_path);

    return true;
}

std::vector<DocumentManager::DocListItem>
DocumentManager::list_documents(uint32_t userId) {
    std::lock_guard lock(mutex_);

    auto docIds = access_.list_documents_for_user(userId);
    std::vector<DocListItem> result;
    result.reserve(docIds.size());

    for (auto docId : docIds) {
        std::string title;
        int online = 0;

        auto tit = titles_.find(docId);
        if (tit != titles_.end()) {
            title = tit->second;
        }

        auto dit = documents_.find(docId);
        if (dit != documents_.end()) {
            online = dit->second->subscriber_count();
        }

        result.push_back({docId, title,
                          role_to_string(docId, userId), online});
    }
    return result;
}

std::vector<std::shared_ptr<collab::Document>>
DocumentManager::all_loaded_documents() {
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<collab::Document>> result;

    result.reserve(documents_.size());

    for (auto& [id, doc] : documents_) {
        result.push_back(doc);
    }

    return result;
}

std::shared_ptr<DocumentSession>
DocumentManager::get_or_create_session(uint32_t docId) {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(docId);

    if (it != sessions_.end()) return it->second;

    auto dit = documents_.find(docId);
    if (dit == documents_.end()) return nullptr;

    auto session = std::make_shared<DocumentSession>(dit->second, logger_);
    session->start();
    sessions_[docId] = session;
    return session;
}

std::shared_ptr<DocumentSession>
DocumentManager::get_session(uint32_t docId) {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(docId);
    return it != sessions_.end() ? it->second : nullptr;
}

void DocumentManager::remove_session(uint32_t docId) {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(docId);

    if (it != sessions_.end()) {
        it->second->request_stop();
        sessions_.erase(it);
    }
}

void DocumentManager::update_cursor(uint32_t docId, CursorState state) {
    std::lock_guard lock(mutex_);
    cursor_buffers_[docId][state.userId] = std::move(state);
}

std::unordered_map<uint32_t, CursorState>
DocumentManager::get_cursors(uint32_t docId) {
    std::lock_guard lock(mutex_);
    auto it = cursor_buffers_.find(docId);

    if (it == cursor_buffers_.end()) return {};

    return it->second;
}

void DocumentManager::remove_cursor(uint32_t docId, uint32_t userId) {
    std::lock_guard lock(mutex_);
    auto it = cursor_buffers_.find(docId);

    if (it != cursor_buffers_.end()) {
        it->second.erase(userId);
    }
}

std::vector<uint32_t> DocumentManager::all_active_doc_ids() {
    std::lock_guard lock(mutex_);
    std::vector<uint32_t> ids;

    for (auto& [id, _] : sessions_) {
        ids.push_back(id);
    }

    return ids;
}

std::shared_ptr<collab::Document>
DocumentManager::load_from_disk_unlocked(uint32_t docId) {
    auto path = std::format("{}/documents/{}.txt", data_dir_, docId);
    if (!fs::exists(path)) return nullptr;

    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    std::string title = load_meta(docId);
    if (title.empty()) {
        title = std::format("Document {}", docId);
    }

    auto doc = std::make_shared<collab::Document>(docId, title);
    doc->load_content(content);
    return doc;
}

std::string DocumentManager::role_to_string(uint32_t docId, uint32_t userId) {
    auto role = access_.get_role(docId, userId);

    if (!role) return "none";

    switch (*role) {
    case collab::Role::Owner: return "owner";
    case collab::Role::Editor: return "editor";
    case collab::Role::Viewer: return "viewer";
    }

    return "none";
}

void DocumentManager::save_meta(uint32_t docId, const std::string& title) {
    auto path = std::format("{}/documents/{}.meta", data_dir_, docId);

    std::ofstream file(path);

    if (file.is_open()) {
        file << title;
    }
}

std::string DocumentManager::load_meta(uint32_t docId) {
    auto path = std::format("{}/documents/{}.meta", data_dir_, docId);
    std::ifstream file(path);

    if (!file.is_open()) return "";

    std::string title;
    std::getline(file, title);

    return title;
}

}
