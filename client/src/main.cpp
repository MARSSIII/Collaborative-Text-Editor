#include "client/connection_dialog.h"
#include "client/network_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("collab");
    QCoreApplication::setApplicationName("collab-client");

    auto* nm = new collab_client::NetworkManager(&app);
    nm->start();

    collab_client::ConnectionDialog dialog(nm);
    if (dialog.exec() != QDialog::Accepted) {
        return 0;
    }

    qInfo() << "[auth] logged in as" << nm->username()
            << "userId=" << nm->userId();

    // Phase 3+ will replace this stub with DocumentListWindow.
    return app.exec();
}
