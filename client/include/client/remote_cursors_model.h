#pragma once

#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <QColor>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace collab_client {

struct RemoteCursor {
    uint32_t userId = 0;
    QString username;
    QColor color;
    uint32_t position = 0;
    std::optional<uint32_t> selectionStart;
    std::optional<uint32_t> selectionEnd;
};

class RemoteCursorsModel : public QObject {
    Q_OBJECT
public:
    explicit RemoteCursorsModel(uint32_t local_user_id, QObject* parent = nullptr);

    std::vector<RemoteCursor> cursors() const;

    void applyBroadcast(const server::CursorBroadcastMsg& msg);
    void applyRemoteOperation(const collab::Operation& op);
    void removeUser(uint32_t userId);
    void clear();

signals:
    void cursorsChanged();

private:
    uint32_t local_user_id_;
    std::unordered_map<uint32_t, RemoteCursor> cursors_;
};

} // namespace collab_client
