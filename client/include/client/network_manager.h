#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <QMutex>

#include <atomic>
#include <cstdint>

class QTcpSocket;
class QThread;

namespace collab_client {

class NetworkManager : public QObject {
    Q_OBJECT

public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() override;

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    void start();

    uint32_t userId() const noexcept { return user_id_.load(std::memory_order_acquire); }
    QString username() const;
    void setIdentity(uint32_t userId, const QString& username);

signals:
    void connected();
    void disconnected(QString reason);
    void messageReceived(QByteArray payload);
    void errorOccurred(QString message);

public slots:
    void connectToHost(QString host, quint16 port);
    void sendFrame(QByteArray payload);
    void disconnectFromHost();

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onSocketError();

private:
    void ensureSocket();

    QThread* thread_ = nullptr;
    QTcpSocket* socket_ = nullptr;
    QByteArray rx_buffer_;

    std::atomic<uint32_t> user_id_{0};
    mutable QMutex identity_mutex_;
    QString username_;
};

} // namespace collab_client
