#include "client/status_bar_widget.h"

#include "client/theme.h"

#include <QLabel>

namespace collab_client {

StatusBarWidget::StatusBarWidget(QWidget* parent) : QStatusBar(parent) {
    setSizeGripEnabled(false);

    cursor_label_ = new QLabel("1:1");
    cursor_label_->setObjectName("mono");

    revision_label_ = new QLabel("rev 0");
    revision_label_->setObjectName("mono");

    state_label_ = new QLabel("SYN");
    state_label_->setAlignment(Qt::AlignCenter);
    state_label_->setMinimumWidth(44);

    online_label_ = new QLabel("online: 0");
    online_label_->setObjectName("muted");

    addPermanentWidget(cursor_label_);
    addPermanentWidget(revision_label_);
    addPermanentWidget(state_label_);
    addPermanentWidget(online_label_);

    setStateLabel("SYN");
}

void StatusBarWidget::setCursorPosition(int line, int column) {
    cursor_label_->setText(QStringLiteral("%1:%2").arg(line).arg(column));
}

void StatusBarWidget::setRevision(uint32_t revision) {
    revision_label_->setText(QStringLiteral("rev %1").arg(revision));
}

void StatusBarWidget::setStateLabel(const QString& label) {
    state_label_->setText(label);
    state_label_->setStyleSheet(
        QStringLiteral("QLabel { %1 }").arg(theme::chipQss(theme::stateColor(label))));
}

void StatusBarWidget::setOnlineCount(int count) {
    online_label_->setText(QStringLiteral("online: %1").arg(count));
}

}
