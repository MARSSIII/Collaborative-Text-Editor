#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <map>

class QListWidget;
class QLabel;
class QPushButton;

namespace collab_client {

class UserPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit UserPanelWidget(QWidget* parent = nullptr);

    void setLocalUser(uint32_t userId, const QString& username, const QColor& color);
    void upsertUser(uint32_t userId, const QString& username, const QColor& color);
    void removeUser(uint32_t userId);
    void clearRemoteUsers();

signals:
    void shareRequested();

private:
    struct Entry {
        QString username;
        QColor color;
        bool local = false;
    };

    void rebuild();

    QListWidget* list_;
    QLabel* header_;
    QPushButton* share_btn_;
    std::map<uint32_t, Entry> users_;
    uint32_t local_user_id_ = 0;
};

}
