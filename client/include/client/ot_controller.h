#pragma once

#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

namespace collab_client {

class NetworkManager;
class LocalDocument;

// TODO(phase-5): replace Synchronized-only stub with the full
// Synchronized / AwaitingAck / AwaitingAckWithBuffer state machine.
class OTController : public QObject {
    Q_OBJECT

public:
    OTController(NetworkManager* nm,
                 LocalDocument* doc,
                 uint32_t doc_id,
                 QObject* parent = nullptr);

signals:
    void revisionChanged(uint32_t revision);
    void remoteOperationApplied(collab::Operation op);
    void fatalError(QString message);

public slots:
    void onLocalOperations(std::vector<collab::Operation> ops);
    void onNetworkMessage(QByteArray payload);

private:
    void handleAck(const server::OperationAckMsg& msg);
    void handleBroadcast(const server::OperationBroadcastMsg& msg);
    void handleError(const server::ErrorMsg& msg);

    NetworkManager* nm_;
    LocalDocument* doc_;
    uint32_t doc_id_;
};

} // namespace collab_client
