#pragma once

#include "collab_protocol/protocol.h"

#include <QMainWindow>

#include <memory>

namespace collab_client {

class NetworkManager;
class EditorWidget;
class StatusBarWidget;
class OTController;
class LocalDocument;

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
    void onNetworkMessage(QByteArray payload);
    void onRevisionChanged(uint32_t revision);
    void onFatalError(QString message);

private:
    NetworkManager* nm_;
    uint32_t doc_id_;

    std::unique_ptr<LocalDocument> local_doc_;
    OTController* ot_ = nullptr;
    EditorWidget* editor_ = nullptr;
    StatusBarWidget* status_ = nullptr;
};

} // namespace collab_client
