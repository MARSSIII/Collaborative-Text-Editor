#include "client/editor_window.h"

#include "client/editor_widget.h"
#include "client/local_document.h"
#include "client/network_manager.h"
#include "client/ot_controller.h"
#include "client/protocol_codec.h"
#include "client/status_bar_widget.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QMetaObject>
#include <QTextCursor>

namespace collab_client {

EditorWindow::EditorWindow(NetworkManager* nm,
                           const server::DocJoinResponseMsg& initial,
                           QWidget* parent)
    : QMainWindow(parent), nm_(nm), doc_id_(initial.docId) {
    setWindowTitle(QStringLiteral("#%1 — %2")
                       .arg(initial.docId)
                       .arg(QString::fromStdString(initial.title)));
    resize(900, 600);

    local_doc_ = std::make_unique<LocalDocument>(initial.content, initial.revision);

    editor_ = new EditorWidget(this);
    editor_->setIdentity(nm_->userId(), initial.revision);
    editor_->resetContent(QString::fromUtf8(initial.content.data(),
                                            static_cast<int>(initial.content.size())));

    ot_ = new OTController(nm_, local_doc_.get(), doc_id_, initial.revision, this);

    status_ = new StatusBarWidget(this);
    setStatusBar(status_);
    status_->setRevision(initial.revision);
    status_->setOnlineCount(static_cast<int>(initial.users.size()));
    status_->setStateLabel("SYN");

    setCentralWidget(editor_);

    connect(editor_, &EditorWidget::localOperationsGenerated,
            ot_, &OTController::onLocalOperations);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged,
            this, &EditorWindow::onCursorPositionChanged);

    connect(nm_, &NetworkManager::messageReceived,
            this, &EditorWindow::onNetworkMessage);
    connect(ot_, &OTController::revisionChanged,
            this, &EditorWindow::onRevisionChanged);
    connect(ot_, &OTController::remoteOperationApplied,
            editor_, &EditorWidget::applyRemoteOperation);
    connect(ot_, &OTController::stateChanged,
            status_, &StatusBarWidget::setStateLabel);
    connect(ot_, &OTController::fatalError,
            this, &EditorWindow::onFatalError);

    onCursorPositionChanged();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::closeEvent(QCloseEvent* event) {
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray, encode_doc_leave_request(doc_id_)));
    emit leftDocument();
    event->accept();
}

void EditorWindow::onCursorPositionChanged() {
    auto c = editor_->textCursor();
    status_->setCursorPosition(c.blockNumber() + 1, c.columnNumber() + 1);
}

void EditorWindow::onNetworkMessage(QByteArray payload) {
    ot_->onNetworkMessage(payload);
}

void EditorWindow::onRevisionChanged(uint32_t revision) {
    editor_->setRevision(revision);
    status_->setRevision(revision);
}

void EditorWindow::onFatalError(QString message) {
    QMessageBox::critical(this, tr("Document error"), message);
    close();
}

} // namespace collab_client
