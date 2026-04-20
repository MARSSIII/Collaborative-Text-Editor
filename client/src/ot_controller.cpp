#include "client/ot_controller.h"

#include "client/local_document.h"
#include "client/logging.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "collab/ot.h"

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

template <typename T>
std::optional<T> parse_as(const QByteArray& payload) {
    try {
        return nlohmann::json::parse(payload.constData(),
                                     payload.constData() + payload.size()).get<T>();
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

}

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
    switch (parse_envelope(payload).type) {
    case server::MessageType::OperationAck:
        if (auto msg = parse_as<server::OperationAckMsg>(payload);
            msg && msg->docId == doc_id_) handleAck(*msg);
        break;
    case server::MessageType::OperationBroadcast:
        if (auto msg = parse_as<server::OperationBroadcastMsg>(payload);
            msg && msg->docId == doc_id_) handleBroadcast(*msg);
        break;
    case server::MessageType::Error:
        if (auto err = parse_error(payload)) handleError(*err);
        break;
    default:
        break;
    }
}

void OTController::handleAck(const server::OperationAckMsg& msg) {
    qCDebug(logOt) << "ack doc=" << msg.docId << "rev=" << msg.revision
                   << "buffer=" << buffer_.size();
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
    qCDebug(logOt) << "remote ops from user=" << msg.userId
                   << "rev=" << msg.revision
                   << "count=" << msg.ops.size();

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
    qCWarning(logOt) << "server error code=" << QString::fromStdString(msg.code)
                     << "msg=" << QString::fromStdString(msg.message);
    if (msg.code == "revision_too_old") {
        emit fatalError(QStringLiteral("Document out of sync — reopen it."));
    }
}

void OTController::sendOp(const collab::Operation& op) {
    server::OperationMsg msg;
    msg.docId = doc_id_;
    msg.revision = revision_;
    msg.ops.push_back(to_entry(op));
    nm_->send(QByteArray::fromStdString(server::serialize(msg)));
}

void OTController::emitStateLabel() {
    switch (state_) {
    case State::Synchronized:         emit stateChanged(QStringLiteral("SYN")); break;
    case State::AwaitingAck:          emit stateChanged(QStringLiteral("ACK")); break;
    case State::AwaitingAckWithBuffer:emit stateChanged(QStringLiteral("BUF")); break;
    }
}

}
