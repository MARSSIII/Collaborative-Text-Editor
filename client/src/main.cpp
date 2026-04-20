#include "client/network_manager.h"
#include "client/protocol_codec.h"
#include "collab_protocol/protocol.h"

#include <QApplication>
#include <QLabel>
#include <QDebug>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QLabel label("Collaborative Text Editor — Qt5 client (Phase 1 skeleton)");
    label.setMinimumSize(520, 120);
    label.setAlignment(Qt::AlignCenter);
    label.show();

    // Smoke-test wiring of NetworkManager — not interactive yet. A real login
    // flow arrives with ConnectionDialog in Phase 2.
    auto* nm = new collab_client::NetworkManager(&app);
    QObject::connect(nm, &collab_client::NetworkManager::connected, [] {
        qInfo() << "[net] connected";
    });
    QObject::connect(nm, &collab_client::NetworkManager::disconnected,
                     [](const QString& reason) {
        qInfo() << "[net] disconnected:" << reason;
    });
    QObject::connect(nm, &collab_client::NetworkManager::errorOccurred,
                     [](const QString& msg) {
        qWarning() << "[net] error:" << msg;
    });
    QObject::connect(nm, &collab_client::NetworkManager::messageReceived,
                     [](const QByteArray& payload) {
        auto env = collab_client::parse_envelope(payload);
        qInfo() << "[net] message type=" << static_cast<int>(env.type)
                << " payload=" << payload;
    });
    nm->start();

    return app.exec();
}
