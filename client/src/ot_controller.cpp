#include "client/ot_controller.h"

#include "client/local_document.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "collab/ot.h"

#include <QMetaObject>

namespace collab_client {

namespace {

server::OpEntry to_entry(const collab::Operation& op) {
    server::OpEntry entry;
    if (op.type == collab::Operation::Type::Insert) {
        entry.op = "insert";
        entry.pos = op.position;
        entry.text = op.text;
    } else {
        entry.op = "delete";
        entry.pos = op.position;
        entry.len = op.length;
    }
    return entry;
}

collab::Operation from_entry(const server::OpEntry& entry,
                             uint32_t userId,
                             uint32_t revision) {
    if (entry.op == "insert") {
        return collab::make_insert(entry.pos, entry.text, userId, revision);
    }
    return collab::make_delete(entry.pos, entry.len, "", userId, revision);
}

} // namespace

OTController::OTController(NetworkManager* nm,
                           LocalDocument* doc,
                           uint32_t doc_id,
                           uint32_t initial_revision,
                           QObject* parent)
    : QObject(parent),
      nm_(nm),
      doc_(doc),
      doc_id_(doc_id),
      revision_(initial_revision) {}

void OTController::onLocalOperations(std::vector<collab::Operation> ops) {
    bool state_changed = false;
    for (auto& op : ops) {
        if (op.is_noop()) continue;
        op.userId = nm_->userId();
        op.revision = revision_;
        doc_->apply(op);

        if (state_ == State::Synchronized) {
            pending_op_ = op;
            sendOp(op);
            state_ = State::AwaitingAck;
            state_changed = true;
        } else {
            buffer_.push_back(op);
            if (state_ == State::AwaitingAck) {
                state_ = State::AwaitingAckWithBuffer;
                state_changed = true;
            }
        }
    }
    if (state_changed) emitStateLabel();
}

void OTController::onNetworkMessage(QByteArray payload) {
    auto env = parse_envelope(payload);
    switch (env.type) {
    case server::MessageType::OperationAck: {
        try {
            auto msg = nlohmann::json::parse(payload.constData(),
                                             payload.constData() + payload.size())
                           .get<server::OperationAckMsg>();
            if (msg.docId == doc_id_) handleAck(msg);
        } catch (const nlohmann::json::exception&) {}
        break;
    }
    case server::MessageType::OperationBroadcast: {
        try {
            auto msg = nlohmann::json::parse(payload.constData(),
                                             payload.constData() + payload.size())
                           .get<server::OperationBroadcastMsg>();
            if (msg.docId == doc_id_) handleBroadcast(msg);
        } catch (const nlohmann::json::exception&) {}
        break;
    }
    case server::MessageType::Error: {
        if (auto err = parse_error(payload)) handleError(*err);
        break;
    }
    default:
        break;
    }
}

void OTController::handleAck(const server::OperationAckMsg& msg) {
    revision_ = msg.revision;
    doc_->set_revision(msg.revision);
    emit revisionChanged(msg.revision);

    if (state_ == State::AwaitingAck) {
        pending_op_.reset();
        state_ = State::Synchronized;
    } else if (state_ == State::AwaitingAckWithBuffer) {
        pending_op_ = buffer_.front();
        buffer_.erase(buffer_.begin());
        sendOp(*pending_op_);
        if (buffer_.empty()) state_ = State::AwaitingAck;
    }
    emitStateLabel();
}

void OTController::handleBroadcast(const server::OperationBroadcastMsg& msg) {
    if (msg.userId == nm_->userId()) return;

    for (const auto& entry : msg.ops) {
        auto server_op = from_entry(entry, msg.userId, msg.revision);

        if (state_ == State::Synchronized) {
            doc_->apply(server_op);
            emit remoteOperationApplied(server_op);
        } else {
            auto [pending_prime, evolved] =
                collab::transform(*pending_op_, server_op);
            pending_op_ = pending_prime;

            for (auto& buffered : buffer_) {
                auto [buffered_prime, next_evolved] =
                    collab::transform(buffered, evolved);
                buffered = buffered_prime;
                evolved = next_evolved;
            }
            doc_->apply(evolved);
            emit remoteOperationApplied(evolved);
        }
    }

    revision_ = msg.revision;
    doc_->set_revision(msg.revision);
    emit revisionChanged(msg.revision);
}

void OTController::handleError(const server::ErrorMsg& msg) {
    if (msg.code == "revision_too_old") {
        emit fatalError(QStringLiteral("Document out of sync — reopen it."));
    }
}

void OTController::sendOp(const collab::Operation& op) {
    server::OperationMsg msg;
    msg.docId = doc_id_;
    msg.revision = revision_;
    msg.ops.push_back(to_entry(op));
    auto payload = QByteArray::fromStdString(server::serialize(msg));
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, payload));
}

void OTController::emitStateLabel() {
    switch (state_) {
    case State::Synchronized:         emit stateChanged(QStringLiteral("SYN")); break;
    case State::AwaitingAck:          emit stateChanged(QStringLiteral("ACK")); break;
    case State::AwaitingAckWithBuffer:emit stateChanged(QStringLiteral("BUF")); break;
    }
}

} // namespace collab_client
