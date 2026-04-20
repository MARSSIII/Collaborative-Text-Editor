#pragma once

#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace collab_client {

class NetworkManager;
class LocalDocument;

class OTController : public QObject {
    Q_OBJECT

public:
    enum class State { Synchronized, AwaitingAck, AwaitingAckWithBuffer };

    OTController(NetworkManager* nm,
                 LocalDocument* doc,
                 uint32_t doc_id,
                 uint32_t initial_revision,
                 QObject* parent = nullptr);

    State state() const noexcept { return state_; }
    uint32_t revision() const noexcept { return revision_; }

signals:
    void revisionChanged(uint32_t revision);
    void remoteOperationApplied(collab::Operation op);
    void stateChanged(QString label);
    void fatalError(QString message);

public slots:
    void onLocalOperations(std::vector<collab::Operation> ops);
    void onNetworkMessage(QByteArray payload);

private:
    void handleAck(const server::OperationAckMsg& msg);
    void handleBroadcast(const server::OperationBroadcastMsg& msg);
    void handleError(const server::ErrorMsg& msg);
    void sendOp(const collab::Operation& op);
    void emitStateLabel();

    NetworkManager* nm_;
    LocalDocument* doc_;
    uint32_t doc_id_;
    uint32_t revision_;

    State state_ = State::Synchronized;
    std::optional<collab::Operation> pending_op_;
    std::vector<collab::Operation> buffer_;
};

} // namespace collab_client
