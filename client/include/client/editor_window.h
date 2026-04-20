#pragma once

#include "collab/operation.h"
#include "collab_protocol/protocol.h"

#include <QMainWindow>

#include <memory>
#include <set>

class QTimer;

namespace collab_client {

class NetworkManager;
class EditorWidget;
class StatusBarWidget;
class OTController;
class LocalDocument;
class RemoteCursorsModel;
class UserPanelWidget;

class EditorWindow : public QMainWindow {
    Q_OBJECT

public:
    EditorWindow(NetworkManager* nm,
                 const server::DocJoinResponseMsg& initial,
                 QWidget* parent = nullptr);
    ~EditorWindow() override;

signals:
    void leftDocument();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onCursorPositionChanged();
    void onLocalSelectionChanged();
    void onNetworkMessage(QByteArray payload);
    void onRevisionChanged(uint32_t revision);
    void onFatalError(QString message);
    void onCursorBroadcastTimeout();
    void onRemoteCursorsChanged();
    void onRemoteOperationApplied(const collab::Operation& op);
    void onShareClicked();

private:
    void scheduleCursorBroadcast();

    NetworkManager* nm_;
    uint32_t doc_id_;

    std::unique_ptr<LocalDocument> local_doc_;
    std::unique_ptr<RemoteCursorsModel> remote_cursors_;
    OTController* ot_ = nullptr;
    EditorWidget* editor_ = nullptr;
    StatusBarWidget* status_ = nullptr;
    UserPanelWidget* user_panel_ = nullptr;
    QTimer* cursor_broadcast_timer_ = nullptr;

    std::set<uint32_t> online_user_ids_;
    bool cursor_dirty_ = false;
};

} // namespace collab_client
