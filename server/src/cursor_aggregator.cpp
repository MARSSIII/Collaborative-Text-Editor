#include "server/cursor_aggregator.h"
#include "collab_protocol/protocol.h"

#include <format>

namespace server {

CursorAggregator::CursorAggregator(DocumentManager& doc_manager,
                                   std::chrono::milliseconds interval,
                                   AsyncLogger& logger)
    : doc_manager_(doc_manager), interval_(interval), logger_(logger) {}

CursorAggregator::~CursorAggregator() {
    stop();
}

void CursorAggregator::start() {
    logger_.info("Cursor aggregator started");
    thread_ = std::jthread([this](std::stop_token stop) {
        broadcast_loop(stop);
    });
}

void CursorAggregator::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        thread_.join();
    }
}

void CursorAggregator::broadcast_loop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(interval_);
        if (stop.stop_requested()) break;

        auto doc_ids = doc_manager_.all_active_doc_ids();

        for (auto docId : doc_ids) {
            auto cursors = doc_manager_.get_cursors(docId);
            if (cursors.empty()) continue;

            auto doc_session = doc_manager_.get_session(docId);
            if (!doc_session) continue;

            CursorBroadcastMsg msg;
            msg.docId = docId;
            for (auto& [userId, state] : cursors) {
                msg.cursors.push_back({
                    state.userId, state.username, state.color,
                    state.position, state.selectionStart, state.selectionEnd
                });
            }

            auto connected = doc_session->connected_users();
            auto payload = serialize(msg);
            for (auto& user : connected) {
                if (auto session = user.session.lock()) {
                    session->send(payload);
                }
            }
        }
    }
}

} // namespace server
