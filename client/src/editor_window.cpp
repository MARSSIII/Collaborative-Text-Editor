#include "client/editor_window.h"

#include "client/editor_widget.h"
#include "client/local_document.h"
#include "client/network_manager.h"
#include "client/ot_controller.h"
#include "client/protocol_codec.h"
#include "client/remote_cursors_model.h"
#include "client/status_bar_widget.h"
#include "client/user_panel_widget.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QTextCursor>
#include <QTimer>

#include <nlohmann/json.hpp>

namespace collab_client {

namespace {
constexpr int kCursorBroadcastIntervalMs = 80;
}

EditorWindow::EditorWindow(NetworkManager* nm,
                           const server::DocJoinResponseMsg& initial,
                           QWidget* parent)
    : QMainWindow(parent), nm_(nm), doc_id_(initial.docId) {
    setWindowTitle(QStringLiteral("#%1 — %2")
                       .arg(initial.docId)
                       .arg(QString::fromStdString(initial.title)));
    resize(1100, 680);

    local_doc_ = std::make_unique<LocalDocument>(initial.content, initial.revision);
    remote_cursors_ = std::make_unique<RemoteCursorsModel>(nm_->userId());

    editor_ = new EditorWidget(this);
    editor_->setIdentity(nm_->userId(), initial.revision);
    editor_->resetContent(QString::fromUtf8(initial.content.data(),
                                            static_cast<int>(initial.content.size())));

    ot_ = new OTController(nm_, local_doc_.get(), doc_id_, initial.revision, this);

    online_user_ids_.insert(nm_->userId());
    for (const auto& u : initial.users) online_user_ids_.insert(u.userId);

    user_panel_ = new UserPanelWidget;
    QString local_color_hex;
    QString local_username = nm_->username();
    for (const auto& u : initial.users) {
        if (u.userId == nm_->userId()) {
            local_color_hex = QString::fromStdString(u.color);
            local_username = QString::fromStdString(u.username);
        }
    }
    user_panel_->setLocalUser(nm_->userId(), local_username,
                              local_color_hex.isEmpty() ? QColor("#808080")
                                                        : QColor(local_color_hex));
    for (const auto& u : initial.users) {
        if (u.userId == nm_->userId()) continue;
        user_panel_->upsertUser(u.userId, QString::fromStdString(u.username),
                                QColor(QString::fromStdString(u.color)));
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

    setCentralWidget(editor_);

    cursor_broadcast_timer_ = new QTimer(this);
    cursor_broadcast_timer_->setInterval(kCursorBroadcastIntervalMs);
    cursor_broadcast_timer_->setSingleShot(true);
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

    onCursorPositionChanged();
    scheduleCursorBroadcast();
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
    auto sel = editor_->localSelectionUtf8();
    std::optional<uint32_t> sel_start, sel_end;
    if (sel) { sel_start = sel->first; sel_end = sel->second; }
    QMetaObject::invokeMethod(nm_, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(QByteArray,
                                    encode_cursor_update(doc_id_, pos, sel_start, sel_end)));
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

    auto env = parse_envelope(payload);
    if (env.type == server::MessageType::UserJoined) {
        if (auto msg = parse_user_joined(payload); msg && msg->docId == doc_id_) {
            online_user_ids_.insert(msg->userId);
            status_->setOnlineCount(static_cast<int>(online_user_ids_.size()));
            user_panel_->upsertUser(msg->userId,
                                    QString::fromStdString(msg->username),
                                    QColor(QString::fromStdString(msg->color)));
            status_->showMessage(tr("%1 joined")
                                     .arg(QString::fromStdString(msg->username)),
                                 2000);
        }
    } else if (env.type == server::MessageType::UserLeft) {
        if (auto msg = parse_user_left(payload); msg && msg->docId == doc_id_) {
            online_user_ids_.erase(msg->userId);
            status_->setOnlineCount(static_cast<int>(online_user_ids_.size()));
            user_panel_->removeUser(msg->userId);
            remote_cursors_->removeUser(msg->userId);
            status_->showMessage(tr("%1 left")
                                     .arg(QString::fromStdString(msg->username)),
                                 2000);
        }
    } else if (env.type == server::MessageType::CursorBroadcast) {
        if (auto msg = parse_cursor_broadcast(payload); msg && msg->docId == doc_id_) {
            remote_cursors_->applyBroadcast(*msg);
        }
    }
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
