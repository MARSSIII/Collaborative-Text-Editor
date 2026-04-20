#pragma once

#include <QStatusBar>

class QLabel;

namespace collab_client {

class StatusBarWidget : public QStatusBar {
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget* parent = nullptr);

public slots:
    void setCursorPosition(int line, int column);
    void setRevision(uint32_t revision);
    void setStateLabel(const QString& label);
    void setOnlineCount(int count);

private:
    QLabel* cursor_label_;
    QLabel* revision_label_;
    QLabel* state_label_;
    QLabel* online_label_;
};

} // namespace collab_client
