#include "client/connection_dialog.h"
#include "client/document_list_window.h"
#include "client/editor_window.h"
#include "client/network_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>

#include <memory>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("collab");
    QCoreApplication::setApplicationName("collab-client");

    auto nm_holder = std::make_unique<collab_client::NetworkManager>();
    auto* nm = nm_holder.get();
    nm->start();

    collab_client::ConnectionDialog dialog(nm);
    if (dialog.exec() != QDialog::Accepted) {
        return 0;
    }

    qInfo() << "[auth] logged in as" << nm->username()
            << "userId=" << nm->userId();

    auto* docs = new collab_client::DocumentListWindow(nm);
    docs->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(docs, &collab_client::DocumentListWindow::documentJoined,
                     [nm, docs](const server::DocJoinResponseMsg& resp) {
        auto* editor = new collab_client::EditorWindow(nm, resp);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(editor, &collab_client::EditorWindow::leftDocument,
                         docs, &QWidget::show);
        docs->hide();
        editor->show();
    });

    docs->show();
    return app.exec();
}
