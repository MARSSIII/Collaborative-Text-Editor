#include "client/connection_dialog.h"
#include "client/document_list_window.h"
#include "client/editor_window.h"
#include "client/logging.h"
#include "client/network_manager.h"
#include "client/theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QLoggingCategory>
#include <QStyleFactory>

#include <memory>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("collab");
    QCoreApplication::setApplicationName("collab-client");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(collab_client::theme::stylesheet());

    if (qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "collab.*.info=true\n"
            "collab.*.warning=true\n"
            "collab.*.critical=true\n"
            "collab.*.debug=false"));
    }

    auto nm_holder = std::make_unique<collab_client::NetworkManager>();
    auto* nm = nm_holder.get();
    nm->start();

    collab_client::ConnectionDialog dialog(nm);
    if (dialog.exec() != QDialog::Accepted) {
        qCInfo(logAuth) << "connection dialog cancelled";
        return 0;
    }

    qCInfo(logAuth) << "session started user=" << nm->username()
                    << "userId=" << nm->userId();

    auto* docs = new collab_client::DocumentListWindow(nm);
    docs->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(docs, &QObject::destroyed, &app, &QApplication::quit);

    QObject::connect(docs, &collab_client::DocumentListWindow::documentJoined,
                     [nm, docs](const server::DocJoinResponseMsg& resp) {
        auto* editor = new collab_client::EditorWindow(nm, resp);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(editor, &collab_client::EditorWindow::leftDocument,
                         docs, [docs]() {
                             docs->show();
                             docs->refresh();
                         });
        docs->hide();
        editor->show();
    });

    docs->show();
    return app.exec();
}
