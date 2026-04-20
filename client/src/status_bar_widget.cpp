#include "client/status_bar_widget.h"

#include <QLabel>

namespace collab_client {

StatusBarWidget::StatusBarWidget(QWidget* parent) : QStatusBar(parent) {
    cursor_label_ = new QLabel("1:1");
    revision_label_ = new QLabel("rev 0");
    state_label_ = new QLabel("SYN");
    online_label_ = new QLabel("online: 0");

    addPermanentWidget(cursor_label_);
    addPermanentWidget(revision_label_);
    addPermanentWidget(state_label_);
    addPermanentWidget(online_label_);
}

void StatusBarWidget::setCursorPosition(int line, int column) {
    cursor_label_->setText(QStringLiteral("%1:%2").arg(line).arg(column));
}

void StatusBarWidget::setRevision(uint32_t revision) {
    revision_label_->setText(QStringLiteral("rev %1").arg(revision));
}

void StatusBarWidget::setStateLabel(const QString& label) {
    state_label_->setText(label);
}

void StatusBarWidget::setOnlineCount(int count) {
    online_label_->setText(QStringLiteral("online: %1").arg(count));
}

}
