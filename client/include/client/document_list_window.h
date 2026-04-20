#pragma once

#include "collab_protocol/protocol.h"

#include <QMainWindow>

class QListWidget;
class QPushButton;
class QLabel;

namespace collab_client {

class NetworkManager;

class DocumentListWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit DocumentListWindow(NetworkManager* nm, QWidget* parent = nullptr);

signals:
    void documentJoined(server::DocJoinResponseMsg response);

public slots:
    void refresh();

private slots:
    void onNewClicked();
    void onOpenClicked();
    void onDeleteClicked();
    void onShareClicked();
    void onMessageReceived(QByteArray payload);
    void onDisconnected(QString reason);

private:
    void requestJoin(uint32_t docId);
    void applyDocList(const server::DocListResponseMsg& msg);

    NetworkManager* nm_;

    QListWidget* list_;
    QPushButton* new_btn_;
    QPushButton* open_btn_;
    QPushButton* delete_btn_;
    QPushButton* share_btn_;
    QPushButton* refresh_btn_;
    QLabel* status_;

    uint32_t pending_join_doc_id_ = 0;
};

}
