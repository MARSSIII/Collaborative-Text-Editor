#include "client/network_manager.h"

#include "client/logging.h"
#include "collab_protocol/protocol.h"

#include <QAbstractSocket>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTcpSocket>
#include <QThread>

namespace collab_client {

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent), thread_(new QThread) {}

QString NetworkManager::username() const {
    QMutexLocker lock(&identity_mutex_);
    return username_;
}

void NetworkManager::setIdentity(uint32_t userId, const QString& username) {
    {
        QMutexLocker lock(&identity_mutex_);
        username_ = username;
    }
    user_id_.store(userId, std::memory_order_release);
}

NetworkManager::~NetworkManager() {
    if (thread_) {
        QMetaObject::invokeMethod(this, "disconnectFromHost", Qt::QueuedConnection);
        thread_->quit();
        thread_->wait();
        delete thread_;
    }
}

void NetworkManager::start() {
    setParent(nullptr);
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
    connect(socket_, &QAbstractSocket::errorOccurred,
            this, &NetworkManager::onSocketError);
}

void NetworkManager::connectToHost(QString host, quint16 port) {
    qCInfo(logNet) << "connecting to" << host << ":" << port;
    ensureSocket();
    rx_buffer_.clear();
    socket_->connectToHost(host, port);
}

void NetworkManager::disconnectFromHost() {
    if (!socket_) return;
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        qCInfo(logNet) << "disconnecting from host";
        socket_->disconnectFromHost();
    }
}

void NetworkManager::sendFrame(QByteArray payload) {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        qCWarning(logNet) << "sendFrame called while disconnected ("
                          << payload.size() << " bytes dropped)";
        emit errorOccurred(QStringLiteral("Not connected"));
        return;
    }
    auto frame = server::encode_frame(
        std::string(payload.constData(), static_cast<size_t>(payload.size())));
    socket_->write(reinterpret_cast<const char*>(frame.data()),
                   static_cast<qint64>(frame.size()));
    qCDebug(logNet) << "sent frame" << payload.size() << "bytes";
}

void NetworkManager::onSocketConnected() {
    qCInfo(logNet) << "socket connected";
    emit connected();
}

void NetworkManager::onSocketDisconnected() {
    qCInfo(logNet) << "socket disconnected";
    rx_buffer_.clear();
    emit disconnected(QStringLiteral("Socket closed"));
}

void NetworkManager::onSocketError() {
    if (socket_) {
        qCWarning(logNet) << "socket error:" << socket_->errorString();
        emit errorOccurred(socket_->errorString());
    }
}

void NetworkManager::onReadyRead() {
    if (!socket_) return;
    rx_buffer_.append(socket_->readAll());

    while (rx_buffer_.size() >= 4) {
        auto len_opt = server::decode_frame_header(
            reinterpret_cast<const uint8_t*>(rx_buffer_.constData()));
        if (!len_opt) {
            qCCritical(logNet) << "frame exceeds 16 MiB — dropping connection";
            emit errorOccurred(QStringLiteral("Frame exceeds 16 MiB limit"));
            disconnectFromHost();
            return;
        }
        const auto payload_len = static_cast<int>(*len_opt);
        if (rx_buffer_.size() < 4 + payload_len) {
            return;
        }
        QByteArray payload = rx_buffer_.mid(4, payload_len);
        rx_buffer_.remove(0, 4 + payload_len);
        qCDebug(logNet) << "received frame" << payload_len << "bytes";
        emit messageReceived(payload);
    }
}

}
