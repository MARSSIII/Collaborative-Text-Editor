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

// Owns its own QThread. Public signals/slots are invoked via queued connections
// — callers (GUI code) never touch the QTcpSocket directly.
class NetworkManager : public QObject {
    Q_OBJECT

public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() override;

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Starts the internal QThread. Must be called before issuing any connect/send.
    void start();

    // Populated by ConnectionDialog on successful auth. Safe to read from any
    // thread (GUI/Network/OTController).
    uint32_t userId() const noexcept { return user_id_.load(std::memory_order_acquire); }
    QString username() const;
    void setIdentity(uint32_t userId, const QString& username);

signals:
    void connected();
    void disconnected(QString reason);
    void messageReceived(QByteArray payload);
    void errorOccurred(QString message);

public slots:
    // All marshalled into thread_ via Qt::QueuedConnection. Safe to invoke
    // from the GUI thread.
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
