#include "client/ot_controller.h"

#include "client/local_document.h"
#include "client/network_manager.h"
#include "client/protocol_codec.h"

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

} // namespace

OTController::OTController(NetworkManager* nm,
                           LocalDocument* doc,
                           uint32_t doc_id,
                           QObject* parent)
    : QObject(parent), nm_(nm), doc_(doc), doc_id_(doc_id) {}

void OTController::onLocalOperations(std::vector<collab::Operation> ops) {
    if (ops.empty()) return;

    // Phase 4: optimistic — apply straight to LocalDocument, send to server.
    // Phase 5 will route through the state machine instead.
    for (const auto& op : ops) {
        if (!op.is_noop()) doc_->apply(op);
    }

    server::OperationMsg msg;
    msg.docId = doc_id_;
    msg.revision = doc_->revision();
    msg.ops.reserve(ops.size());
    for (const auto& op : ops) {
        if (!op.is_noop()) msg.ops.push_back(to_entry(op));
    }
    if (msg.ops.empty()) return;

    auto payload = QByteArray::fromStdString(server::serialize(msg));
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, payload));
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
    doc_->set_revision(msg.revision);
    emit revisionChanged(msg.revision);
}

void OTController::handleBroadcast(const server::OperationBroadcastMsg& msg) {
    // Phase 4 stub: silently drop broadcasts from other users. Phase 5 will
    // feed them through the full OT state machine.
    (void)msg;
}

void OTController::handleError(const server::ErrorMsg& msg) {
    if (msg.code == "revision_too_old") {
        emit fatalError(QStringLiteral("Document out of sync — reopen it."));
    }
}

} // namespace collab_client
