#include "client/network_manager.h"
#include "collab_protocol/protocol.h"

#include <QtTest/QtTest>
#include <QByteArray>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

using namespace collab_client;

namespace {

QByteArray make_frame(const QByteArray& payload) {
    const auto len = static_cast<quint32>(payload.size());
    QByteArray out;
    out.resize(4);
    out[0] = static_cast<char>((len >> 24) & 0xFF);
    out[1] = static_cast<char>((len >> 16) & 0xFF);
    out[2] = static_cast<char>((len >> 8) & 0xFF);
    out[3] = static_cast<char>(len & 0xFF);
    out.append(payload);
    return out;
}

QByteArray oversized_frame_header(quint32 claimed_len) {
    QByteArray out(4, '\0');
    out[0] = static_cast<char>((claimed_len >> 24) & 0xFF);
    out[1] = static_cast<char>((claimed_len >> 16) & 0xFF);
    out[2] = static_cast<char>((claimed_len >> 8) & 0xFF);
    out[3] = static_cast<char>(claimed_len & 0xFF);
    return out;
}

}

class NetworkManagerTest : public QObject {
    Q_OBJECT

private slots:
    void init() {
        server_ = new QTcpServer(this);
        QVERIFY(server_->listen(QHostAddress::LocalHost, 0));
        port_ = server_->serverPort();

        nm_ = new NetworkManager();
        nm_->start();
    }

    void cleanup() {
        delete nm_;
        nm_ = nullptr;
        delete server_;
        server_ = nullptr;
    }

    void connectsAndReceivesSingleFrame() {
        QSignalSpy connected_spy(nm_, &NetworkManager::connected);
        QSignalSpy msg_spy(nm_, &NetworkManager::messageReceived);

        QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port_));

        QVERIFY(server_->waitForNewConnection(2000));
        auto* server_socket = server_->nextPendingConnection();
        QVERIFY(server_socket);

        QTRY_COMPARE_WITH_TIMEOUT(connected_spy.count(), 1, 2000);

        auto frame = make_frame(QByteArrayLiteral(R"({"type":"ping"})"));
        server_socket->write(frame);
        server_socket->flush();

        QTRY_COMPARE_WITH_TIMEOUT(msg_spy.count(), 1, 2000);
        QCOMPARE(msg_spy.takeFirst().at(0).toByteArray(),
                 QByteArray(R"({"type":"ping"})"));
    }

    void parsesTwoFramesInOneReadyRead() {
        QSignalSpy connected_spy(nm_, &NetworkManager::connected);
        QSignalSpy msg_spy(nm_, &NetworkManager::messageReceived);

        QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port_));

        QVERIFY(server_->waitForNewConnection(2000));
        auto* server_socket = server_->nextPendingConnection();
        QTRY_COMPARE_WITH_TIMEOUT(connected_spy.count(), 1, 2000);

        QByteArray batch;
        batch.append(make_frame(QByteArrayLiteral(R"({"type":"a"})")));
        batch.append(make_frame(QByteArrayLiteral(R"({"type":"b"})")));
        server_socket->write(batch);
        server_socket->flush();

        QTRY_COMPARE_WITH_TIMEOUT(msg_spy.count(), 2, 2000);
        QCOMPARE(msg_spy.at(0).at(0).toByteArray(), QByteArray(R"({"type":"a"})"));
        QCOMPARE(msg_spy.at(1).at(0).toByteArray(), QByteArray(R"({"type":"b"})"));
    }

    void rejectsOversizedFrame() {
        QSignalSpy connected_spy(nm_, &NetworkManager::connected);
        QSignalSpy error_spy(nm_, &NetworkManager::errorOccurred);

        QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port_));

        QVERIFY(server_->waitForNewConnection(2000));
        auto* server_socket = server_->nextPendingConnection();
        QTRY_COMPARE_WITH_TIMEOUT(connected_spy.count(), 1, 2000);

        server_socket->write(oversized_frame_header(server::MAX_PAYLOAD_SIZE + 1));
        server_socket->flush();

        QTRY_VERIFY_WITH_TIMEOUT(error_spy.count() >= 1, 2000);
        QVERIFY(error_spy.first().at(0).toString().contains("16 MiB"));
    }

    void reconnectsAfterDisconnect() {
        QSignalSpy connected_spy(nm_, &NetworkManager::connected);
        QSignalSpy disconnect_spy(nm_, &NetworkManager::disconnected);

        QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port_));
        QVERIFY(server_->waitForNewConnection(2000));
        auto* first = server_->nextPendingConnection();
        QTRY_COMPARE_WITH_TIMEOUT(connected_spy.count(), 1, 2000);

        first->disconnectFromHost();
        QTRY_VERIFY_WITH_TIMEOUT(disconnect_spy.count() >= 1, 2000);

        QMetaObject::invokeMethod(nm_, "connectToHost", Qt::QueuedConnection,
                                  Q_ARG(QString, "127.0.0.1"), Q_ARG(quint16, port_));
        QVERIFY(server_->waitForNewConnection(2000));
        auto* second = server_->nextPendingConnection();
        QVERIFY(second);
        QTRY_VERIFY_WITH_TIMEOUT(connected_spy.count() >= 2, 2000);
    }

private:
    QTcpServer* server_ = nullptr;
    NetworkManager* nm_ = nullptr;
    quint16 port_ = 0;
};

QTEST_MAIN(NetworkManagerTest)
#include "test_network_manager.moc"
