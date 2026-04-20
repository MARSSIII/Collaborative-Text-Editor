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

private slots:
    void refresh();
    void onNewClicked();
    void onOpenClicked();
    void onDeleteClicked();
    void onMessageReceived(QByteArray payload);

private:
    void requestJoin(uint32_t docId);
    void applyDocList(const server::DocListResponseMsg& msg);

    NetworkManager* nm_;

    QListWidget* list_;
    QPushButton* new_btn_;
    QPushButton* open_btn_;
    QPushButton* delete_btn_;
    QPushButton* refresh_btn_;
    QLabel* status_;

    // Track the docId being joined — so if the user clicks Open twice fast,
    // only the first join_response dispatches and the second is ignored.
    uint32_t pending_join_doc_id_ = 0;
};

} // namespace collab_client
