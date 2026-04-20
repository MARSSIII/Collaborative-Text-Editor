#include "client/user_panel_widget.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace collab_client {

namespace {

QIcon dot_icon(const QColor& color) {
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, 12, 12);
    return QIcon(pm);
}

}

UserPanelWidget::UserPanelWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    header_ = new QLabel(tr("Online"));
    auto f = header_->font();
    f.setBold(true);
    header_->setFont(f);
    list_ = new QListWidget;
    list_->setFocusPolicy(Qt::NoFocus);
    share_btn_ = new QPushButton(tr("Share…"));
    share_btn_->setToolTip(tr("Grant editor access to another user"));
    connect(share_btn_, &QPushButton::clicked, this, &UserPanelWidget::shareRequested);
    layout->addWidget(header_);
    layout->addWidget(list_,  1);
    layout->addWidget(share_btn_);
    setMinimumWidth(180);
}

void UserPanelWidget::setLocalUser(uint32_t userId, const QString& username, const QColor& color) {
    local_user_id_ = userId;
    users_[userId] = Entry{username, color.isValid() ? color : QColor("#808080"), true};
    rebuild();
}

void UserPanelWidget::upsertUser(uint32_t userId, const QString& username, const QColor& color) {
    auto it = users_.find(userId);
    const bool is_local = (userId == local_user_id_);
    QColor c = color.isValid() ? color : QColor("#808080");
    if (it == users_.end()) {
        users_[userId] = Entry{username, c, is_local};
    } else {
        if (!username.isEmpty()) it->second.username = username;
        if (color.isValid()) it->second.color = c;
        it->second.local = is_local;
    }
    rebuild();
}

void UserPanelWidget::removeUser(uint32_t userId) {
    if (userId == local_user_id_) return;
    if (users_.erase(userId) > 0) rebuild();
}

void UserPanelWidget::clearRemoteUsers() {
    for (auto it = users_.begin(); it != users_.end(); ) {
        if (it->first != local_user_id_) it = users_.erase(it);
        else ++it;
    }
    rebuild();
}

void UserPanelWidget::rebuild() {
    list_->clear();
    header_->setText(tr("Online: %1").arg(users_.size()));
    for (const auto& [id, e] : users_) {
        QString label = e.username;
        if (e.local) label += tr(" (you)");
        auto* item = new QListWidgetItem(dot_icon(e.color), label);
        list_->addItem(item);
    }
}

}
