#include "client/connection_dialog.h"
#include "client/document_list_window.h"
#include "client/network_manager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QMessageBox>

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

    auto* docs = new collab_client::DocumentListWindow(nm);
    docs->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(docs, &collab_client::DocumentListWindow::documentJoined,
                     [docs](const server::DocJoinResponseMsg& resp) {
        // Phase 4 will open an EditorWindow here. For now just report the
        // snapshot to the log so we can prove the flow end-to-end.
        qInfo() << "[doc] joined" << resp.docId
                << "title=" << QString::fromStdString(resp.title)
                << "revision=" << resp.revision
                << "role=" << QString::fromStdString(resp.role)
                << "content-size=" << resp.content.size();
        QMessageBox::information(docs,
                                 QObject::tr("Document opened"),
                                 QObject::tr("Joined document #%1 \"%2\" at revision %3.\n\n"
                                             "EditorWindow arrives in Phase 4.")
                                     .arg(resp.docId)
                                     .arg(QString::fromStdString(resp.title))
                                     .arg(resp.revision));
    });

    docs->show();
    return app.exec();
}
