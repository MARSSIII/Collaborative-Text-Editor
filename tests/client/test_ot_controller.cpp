#include "client/local_document.h"
#include "client/network_manager.h"
#include "client/ot_controller.h"
#include "client/protocol_codec.h"
#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <QtTest/QtTest>
#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <memory>
#include <nlohmann/json.hpp>

Q_DECLARE_METATYPE(std::vector<collab::Operation>)
Q_DECLARE_METATYPE(collab::Operation)

using namespace collab_client;

namespace {

QByteArray build_broadcast(uint32_t docId,
                           uint32_t userId,
                           const std::string& username,
                           uint32_t revision,
                           std::initializer_list<server::OpEntry> ops) {
    server::OperationBroadcastMsg msg;
    msg.docId = docId;
    msg.userId = userId;
    msg.username = username;
    msg.revision = revision;
    msg.ops.assign(ops.begin(), ops.end());
    auto s = server::serialize(msg);
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

QByteArray build_ack(uint32_t docId, uint32_t revision) {
    server::OperationAckMsg msg{docId, revision};
    auto s = server::serialize(msg);
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

QByteArray build_error(const std::string& code, const std::string& message) {
    server::ErrorMsg msg{code, message};
    auto s = server::serialize(msg);
    return QByteArray(s.data(), static_cast<int>(s.size()));
}

}

class OTControllerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        qRegisterMetaType<std::vector<collab::Operation>>(
            "std::vector<collab::Operation>");
        qRegisterMetaType<collab::Operation>("collab::Operation");
    }

    void init() {
        nm_ = std::make_unique<NetworkManager>();
        nm_->setIdentity(1, "me");
        nm_->start();

        doc_ = std::make_unique<LocalDocument>("Hello", 0);
        ot_ = std::make_unique<OTController>(nm_.get(), doc_.get(), 1, 0);
    }

    void cleanup() {
        ot_.reset();
        doc_.reset();
        nm_.reset();
    }

    void startsInSynchronized() {
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::Synchronized));
    }

    void localOpMovesToAwaitingAck() {
        ot_->onLocalOperations({collab::make_insert(5, " world", 1, 0)});
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::AwaitingAck));
        QCOMPARE(doc_->snapshot(), std::string("Hello world"));
    }

    void ackInAwaitingReturnsToSynchronized() {
        ot_->onLocalOperations({collab::make_insert(5, " world", 1, 0)});
        ot_->onNetworkMessage(build_ack(1, 1));
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::Synchronized));
        QCOMPARE(ot_->revision(), 1u);
        QCOMPARE(doc_->revision(), 1u);
    }

    void secondLocalOpMovesToAwaitingAckWithBuffer() {
        ot_->onLocalOperations({collab::make_insert(5, "A", 1, 0)});
        ot_->onLocalOperations({collab::make_insert(6, "B", 1, 0)});
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::AwaitingAckWithBuffer));
        QCOMPARE(doc_->snapshot(), std::string("HelloAB"));
    }

    void ackFromBufferFlushesOne() {
        ot_->onLocalOperations({collab::make_insert(5, "A", 1, 0)});
        ot_->onLocalOperations({collab::make_insert(6, "B", 1, 0)});
        ot_->onNetworkMessage(build_ack(1, 1));
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::AwaitingAck));

        ot_->onNetworkMessage(build_ack(1, 2));
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::Synchronized));
        QCOMPARE(ot_->revision(), 2u);
    }

    void remoteOpInSynchronizedAppliesDirectly() {
        QSignalSpy applied_spy(ot_.get(), &OTController::remoteOperationApplied);
        server::OpEntry ins;
        ins.op = "insert"; ins.pos = 5; ins.text = " world";
        ot_->onNetworkMessage(build_broadcast(1, 2, "bob", 1, {ins}));

        QCOMPARE(applied_spy.count(), 1);
        QCOMPARE(doc_->snapshot(), std::string("Hello world"));
        QCOMPARE(doc_->revision(), 1u);
    }

    void remoteOpInAwaitingAckTransformsPending() {
        ot_->onLocalOperations({collab::make_insert(5, "!", 1, 0)});

        server::OpEntry ins_other;
        ins_other.op = "insert"; ins_other.pos = 0; ins_other.text = "X";
        ot_->onNetworkMessage(build_broadcast(1, 2, "bob", 1, {ins_other}));

        QCOMPARE(doc_->snapshot(), std::string("XHello!"));
        QCOMPARE(doc_->revision(), 1u);
        QCOMPARE(static_cast<int>(ot_->state()),
                 static_cast<int>(OTController::State::AwaitingAck));
    }

    void ownBroadcastIsIgnored() {
        ot_->onLocalOperations({collab::make_insert(5, "!", 1, 0)});
        QCOMPARE(doc_->snapshot(), std::string("Hello!"));

        server::OpEntry echo;
        echo.op = "insert"; echo.pos = 5; echo.text = "!";
        ot_->onNetworkMessage(build_broadcast(1, 1, "me", 1, {echo}));

        QCOMPARE(doc_->snapshot(), std::string("Hello!"));
    }

    void revisionTooOldTriggersFatal() {
        QSignalSpy fatal_spy(ot_.get(), &OTController::fatalError);
        ot_->onNetworkMessage(build_error("revision_too_old", "stale"));
        QCOMPARE(fatal_spy.count(), 1);
    }

    void stateChangedEmittedOnTransitions() {
        QSignalSpy state_spy(ot_.get(), &OTController::stateChanged);
        ot_->onLocalOperations({collab::make_insert(5, "A", 1, 0)});
        ot_->onLocalOperations({collab::make_insert(6, "B", 1, 0)});
        ot_->onNetworkMessage(build_ack(1, 1));
        ot_->onNetworkMessage(build_ack(1, 2));
        QVERIFY(state_spy.count() >= 3);
    }

private:
    std::unique_ptr<NetworkManager> nm_;
    std::unique_ptr<LocalDocument> doc_;
    std::unique_ptr<OTController> ot_;
};

QTEST_MAIN(OTControllerTest)
#include "test_ot_controller.moc"
