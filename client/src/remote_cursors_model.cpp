#include "client/remote_cursors_model.h"

#include "client/cursor_translator.h"

namespace collab_client {

RemoteCursorsModel::RemoteCursorsModel(uint32_t local_user_id, QObject* parent)
    : QObject(parent), local_user_id_(local_user_id) {}

std::vector<RemoteCursor> RemoteCursorsModel::cursors() const {
    std::vector<RemoteCursor> out;
    out.reserve(cursors_.size());
    for (const auto& [_, c] : cursors_) out.push_back(c);
    return out;
}

void RemoteCursorsModel::applyBroadcast(const server::CursorBroadcastMsg& msg) {
    std::unordered_map<uint32_t, RemoteCursor> next;
    next.reserve(msg.cursors.size());
    for (const auto& c : msg.cursors) {
        if (c.userId == local_user_id_) continue;
        RemoteCursor rc;
        rc.userId = c.userId;
        rc.username = QString::fromStdString(c.username);
        rc.color = QColor(QString::fromStdString(c.color));
        if (!rc.color.isValid()) rc.color = QColor("#808080");
        rc.position = c.position;
        rc.selectionStart = c.selectionStart;
        rc.selectionEnd = c.selectionEnd;
        next.emplace(c.userId, std::move(rc));
    }
    cursors_ = std::move(next);
    emit cursorsChanged();
}

void RemoteCursorsModel::applyRemoteOperation(const collab::Operation& op) {
    if (op.is_noop() || cursors_.empty()) return;
    for (auto& [_, c] : cursors_) {
        c.position = cursor_translator::translate(c.position, op);
        if (c.selectionStart) c.selectionStart = cursor_translator::translate(*c.selectionStart, op);
        if (c.selectionEnd) c.selectionEnd = cursor_translator::translate(*c.selectionEnd, op);
    }
    emit cursorsChanged();
}

void RemoteCursorsModel::removeUser(uint32_t userId) {
    if (cursors_.erase(userId) > 0) emit cursorsChanged();
}

void RemoteCursorsModel::clear() {
    if (cursors_.empty()) return;
    cursors_.clear();
    emit cursorsChanged();
}

}
