#include "client/network_manager.h"

#include "collab_protocol/protocol.h"

#include <QAbstractSocket>
#include <QMetaObject>
#include <QTcpSocket>
#include <QThread>

namespace collab_client {

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent), thread_(new QThread) {}

NetworkManager::~NetworkManager() {
    if (thread_) {
        QMetaObject::invokeMethod(this, "disconnectFromHost", Qt::QueuedConnection);
        thread_->quit();
        thread_->wait();
        delete thread_;
    }
}

void NetworkManager::start() {
    moveToThread(thread_);
    thread_->start();
}

void NetworkManager::ensureSocket() {
    if (socket_) return;
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected,
            this, &NetworkManager::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected,
            this, &NetworkManager::onSocketDisconnected);
    connect(socket_, &QTcpSocket::readyRead,
            this, &NetworkManager::onReadyRead);
    connect(socket_,
            static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(
                &QAbstractSocket::error),
            this, &NetworkManager::onSocketError);
}

void NetworkManager::connectToHost(QString host, quint16 port) {
    ensureSocket();
    rx_buffer_.clear();
    socket_->connectToHost(host, port);
}

void NetworkManager::disconnectFromHost() {
    if (!socket_) return;
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }
}

void NetworkManager::sendFrame(QByteArray payload) {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("Not connected"));
        return;
    }
    auto frame = server::encode_frame(
        std::string(payload.constData(), static_cast<size_t>(payload.size())));
    socket_->write(reinterpret_cast<const char*>(frame.data()),
                   static_cast<qint64>(frame.size()));
}

void NetworkManager::onSocketConnected() {
    emit connected();
}

void NetworkManager::onSocketDisconnected() {
    rx_buffer_.clear();
    emit disconnected(QStringLiteral("Socket closed"));
}

void NetworkManager::onSocketError() {
    if (socket_) {
        emit errorOccurred(socket_->errorString());
    }
}

void NetworkManager::onReadyRead() {
    if (!socket_) return;
    rx_buffer_.append(socket_->readAll());

    // A single readyRead may carry multiple frames — drain them all.
    while (rx_buffer_.size() >= 4) {
        auto len_opt = server::decode_frame_header(
            reinterpret_cast<const uint8_t*>(rx_buffer_.constData()));
        if (!len_opt) {
            emit errorOccurred(QStringLiteral("Frame exceeds 16 MiB limit"));
            disconnectFromHost();
            return;
        }
        const auto payload_len = static_cast<int>(*len_opt);
        if (rx_buffer_.size() < 4 + payload_len) {
            return; // wait for the rest of the frame
        }
        QByteArray payload = rx_buffer_.mid(4, payload_len);
        rx_buffer_.remove(0, 4 + payload_len);
        emit messageReceived(payload);
    }
}

} // namespace collab_client
