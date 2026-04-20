#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

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
};

} // namespace collab_client
