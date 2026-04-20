#include "client/editor_window.h"

#include "client/editor_widget.h"
#include "client/local_document.h"
#include "client/logging.h"
#include "client/network_manager.h"
#include "client/ot_controller.h"
#include "client/protocol_codec.h"
#include "client/remote_cursors_model.h"
#include "client/status_bar_widget.h"
#include "client/user_panel_widget.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>

namespace collab_client {

namespace {

constexpr int kCursorBroadcastIntervalMs = 80;
constexpr auto kReadOnlySuffix = " [read-only]";

QColor parseColor(const std::string& hex) {
    QColor c(QString::fromStdString(hex));
    return c.isValid() ? c : QColor("#808080");
}

}

EditorWindow::EditorWindow(NetworkManager* nm,
                           const server::DocJoinResponseMsg& initial,
                           QWidget* parent)
    : QMainWindow(parent),
      nm_(nm),
      doc_id_(initial.docId),
      base_title_(QStringLiteral("#%1 — %2").arg(initial.docId)
                      .arg(QString::fromStdString(initial.title))),
      role_(QString::fromStdString(initial.role)) {
    setWindowTitle(base_title_);
    resize(1100, 680);
    qCInfo(logEditor) << "opened doc=" << initial.docId
                      << "title=" << QString::fromStdString(initial.title)
                      << "rev=" << initial.revision
                      << "role=" << role_;

    setupModels(initial);
    setupUi(initial);
    setupToolbar();
    wireSignals();

    applyRole(role_);
    onCursorPositionChanged();
    scheduleCursorBroadcast();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setupModels(const server::DocJoinResponseMsg& initial) {
    local_doc_ = std::make_unique<LocalDocument>(initial.content, initial.revision);
    remote_cursors_ = std::make_unique<RemoteCursorsModel>(nm_->userId());

    online_user_ids_.insert(nm_->userId());
    for (const auto& u : initial.users) online_user_ids_.insert(u.userId);
}

void EditorWindow::setupUi(const server::DocJoinResponseMsg& initial) {
    editor_ = new EditorWidget(this);
    editor_->setIdentity(nm_->userId(), initial.revision);
    editor_->resetContent(QString::fromUtf8(initial.content.data(),
                                            static_cast<int>(initial.content.size())));
    setCentralWidget(editor_);

    ot_ = new OTController(nm_, local_doc_.get(), doc_id_, initial.revision, this);

    user_panel_ = new UserPanelWidget;
    QString local_username = nm_->username();
    QColor local_color("#808080");
    for (const auto& u : initial.users) {
        if (u.userId == nm_->userId()) {
            local_username = QString::fromStdString(u.username);
            local_color = parseColor(u.color);
            break;
        }
    }
    user_panel_->setLocalUser(nm_->userId(), local_username, local_color);
    for (const auto& u : initial.users) {
        if (u.userId == nm_->userId()) continue;
        user_panel_->upsertUser(u.userId, QString::fromStdString(u.username),
                                parseColor(u.color));
    }

    auto* dock = new QDockWidget(tr("Users"), this);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setWidget(user_panel_);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    status_ = new StatusBarWidget(this);
    setStatusBar(status_);
    status_->setRevision(initial.revision);
    status_->setOnlineCount(static_cast<int>(online_user_ids_.size()));
    status_->setStateLabel("SYN");

    cursor_broadcast_timer_ = new QTimer(this);
    cursor_broadcast_timer_->setInterval(kCursorBroadcastIntervalMs);
    cursor_broadcast_timer_->setSingleShot(true);
}

void EditorWindow::setupToolbar() {
    auto* toolbar = addToolBar(tr("Document"));
    toolbar->setObjectName("DocumentToolBar");
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* leave = toolbar->addAction(tr("← Documents"));
    leave->setShortcut(QKeySequence::Close);
    leave->setToolTip(tr("Return to the document list (Ctrl+W)"));
    connect(leave, &QAction::triggered, this, &QWidget::close);

    auto* share = toolbar->addAction(tr("Share…"));
    share->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    share->setToolTip(tr("Grant editor access to another user (Ctrl+Shift+S)"));
    connect(share, &QAction::triggered, this, &EditorWindow::onShareClicked);
}

void EditorWindow::wireSignals() {
    connect(cursor_broadcast_timer_, &QTimer::timeout,
            this, &EditorWindow::onCursorBroadcastTimeout);

    connect(editor_, &EditorWidget::localOperationsGenerated,
            ot_, &OTController::onLocalOperations);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged,
            this, &EditorWindow::onCursorPositionChanged);
    connect(editor_, &QPlainTextEdit::selectionChanged,
            this, &EditorWindow::onLocalSelectionChanged);

    connect(nm_, &NetworkManager::messageReceived,
            this, &EditorWindow::onNetworkMessage);
    connect(nm_, &NetworkManager::disconnected,
            this, &EditorWindow::onDisconnected);

    connect(ot_, &OTController::revisionChanged,
            this, &EditorWindow::onRevisionChanged);
    connect(ot_, &OTController::remoteOperationApplied,
            this, &EditorWindow::onRemoteOperationApplied);
    connect(ot_, &OTController::stateChanged,
            status_, &StatusBarWidget::setStateLabel);
    connect(ot_, &OTController::fatalError,
            this, &EditorWindow::onFatalError);

    connect(remote_cursors_.get(), &RemoteCursorsModel::cursorsChanged,
            this, &EditorWindow::onRemoteCursorsChanged);
    connect(user_panel_, &UserPanelWidget::shareRequested,
            this, &EditorWindow::onShareClicked);
}

void EditorWindow::closeEvent(QCloseEvent* event) {
    qCInfo(logEditor) << "closing doc=" << doc_id_ << "forced=" << closing_;
    if (!closing_) {
        nm_->send(encode_doc_leave_request(doc_id_));
    }
    emit leftDocument();
    event->accept();
}

void EditorWindow::onCursorPositionChanged() {
    const auto c = editor_->textCursor();
    status_->setCursorPosition(c.blockNumber() + 1, c.columnNumber() + 1);
    scheduleCursorBroadcast();
}

void EditorWindow::onLocalSelectionChanged() {
    scheduleCursorBroadcast();
}

void EditorWindow::scheduleCursorBroadcast() {
    cursor_dirty_ = true;
    if (!cursor_broadcast_timer_->isActive()) cursor_broadcast_timer_->start();
}

void EditorWindow::onCursorBroadcastTimeout() {
    if (!cursor_dirty_) return;
    cursor_dirty_ = false;
    const uint32_t pos = editor_->localCursorUtf8Position();
    const auto sel = editor_->localSelectionUtf8();
    std::optional<uint32_t> sel_start, sel_end;
    if (sel) { sel_start = sel->first; sel_end = sel->second; }
    nm_->send(encode_cursor_update(doc_id_, pos, sel_start, sel_end));
}

void EditorWindow::onRemoteOperationApplied(const collab::Operation& op) {
    editor_->applyRemoteOperation(op);
    remote_cursors_->applyRemoteOperation(op);
}

void EditorWindow::onRemoteCursorsChanged() {
    editor_->setRemoteCursors(remote_cursors_->cursors());
    for (const auto& c : remote_cursors_->cursors()) {
        user_panel_->upsertUser(c.userId, c.username, c.color);
    }
}

void EditorWindow::onNetworkMessage(QByteArray payload) {
    ot_->onNetworkMessage(payload);

    switch (parse_envelope(payload).type) {
    case server::MessageType::UserJoined:      handleUserJoined(payload);      break;
    case server::MessageType::UserLeft:        handleUserLeft(payload);        break;
    case server::MessageType::CursorBroadcast: handleCursorBroadcast(payload); break;
    case server::MessageType::ServerShutdown:  handleServerShutdown(payload);  break;
    case server::MessageType::DocDeleted:      handleDocDeleted(payload);      break;
    case server::MessageType::RoleChanged:     handleRoleChanged(payload);     break;
    default: break;
    }
}

void EditorWindow::handleUserJoined(const QByteArray& payload) {
    auto msg = parse_user_joined(payload);
    if (!msg || msg->docId != doc_id_) return;
    online_user_ids_.insert(msg->userId);
    status_->setOnlineCount(static_cast<int>(online_user_ids_.size()));
    user_panel_->upsertUser(msg->userId,
                            QString::fromStdString(msg->username),
                            parseColor(msg->color));
    status_->showMessage(tr("%1 joined").arg(QString::fromStdString(msg->username)), 2000);
}

void EditorWindow::handleUserLeft(const QByteArray& payload) {
    auto msg = parse_user_left(payload);
    if (!msg || msg->docId != doc_id_) return;
    online_user_ids_.erase(msg->userId);
    status_->setOnlineCount(static_cast<int>(online_user_ids_.size()));
    user_panel_->removeUser(msg->userId);
    remote_cursors_->removeUser(msg->userId);
    status_->showMessage(tr("%1 left").arg(QString::fromStdString(msg->username)), 2000);
}

void EditorWindow::handleCursorBroadcast(const QByteArray& payload) {
    auto msg = parse_cursor_broadcast(payload);
    if (!msg || msg->docId != doc_id_) return;
    remote_cursors_->applyBroadcast(*msg);
}

void EditorWindow::handleServerShutdown(const QByteArray& payload) {
    auto msg = parse_server_shutdown(payload);
    const QString body = msg
        ? QString::fromStdString(msg->message)
        : tr("Server is shutting down");
    qCWarning(logEditor) << "server shutdown:" << body;
    closeWithNotice(tr("Server shutdown"), body);
}

void EditorWindow::handleDocDeleted(const QByteArray& payload) {
    auto msg = parse_doc_deleted(payload);
    if (!msg || msg->docId != doc_id_) return;
    qCWarning(logEditor) << "document deleted doc=" << doc_id_;
    closeWithNotice(tr("Document deleted"),
                    tr("This document was deleted by the owner."));
}

void EditorWindow::handleRoleChanged(const QByteArray& payload) {
    auto msg = parse_role_changed(payload);
    if (!msg || msg->docId != doc_id_) return;
    const auto new_role = QString::fromStdString(msg->newRole);
    qCInfo(logEditor) << "role changed doc=" << doc_id_
                      << "from=" << role_ << "to=" << new_role;
    role_ = new_role;
    applyRole(role_);
    status_->showMessage(tr("Your role is now: %1").arg(role_), 3000);
}

void EditorWindow::onRevisionChanged(uint32_t revision) {
    editor_->setRevision(revision);
    status_->setRevision(revision);
}

void EditorWindow::onFatalError(QString message) {
    QMessageBox::critical(this, tr("Document error"), message);
    close();
}

void EditorWindow::onShareClicked() {
    bool ok = false;
    const auto target = QInputDialog::getText(this, tr("Share document"),
                                              tr("Grant editor access to username:"),
                                              QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || target.isEmpty()) return;

    qCInfo(logEditor) << "share doc=" << doc_id_ << "with=" << target;
    nm_->send(encode_doc_share_request(doc_id_, target, "editor"));
    status_->showMessage(tr("Shared with %1").arg(target), 2500);
}

void EditorWindow::onDisconnected(QString reason) {
    if (closing_) return;
    qCWarning(logEditor) << "disconnected doc=" << doc_id_ << "reason=" << reason;
    closeWithNotice(tr("Connection lost"),
                    tr("Lost connection to server: %1").arg(reason));
}

void EditorWindow::applyRole(const QString& role) {
    const bool read_only = (role != QStringLiteral("owner")
                            && role != QStringLiteral("editor"));
    editor_->setReadOnly(read_only);
    setWindowTitle(read_only ? base_title_ + kReadOnlySuffix : base_title_);
}

void EditorWindow::closeWithNotice(const QString& title, const QString& body) {
    if (closing_) return;
    closing_ = true;
    QMessageBox::information(this, title, body);
    close();
}

}
