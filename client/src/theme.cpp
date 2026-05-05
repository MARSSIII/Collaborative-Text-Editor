#include "client/theme.h"

#include <QLabel>

namespace collab_client::theme {

QString stylesheet() {
    return QStringLiteral(R"(
* {
    font-family: "Inter", "SF Pro Text", "Segoe UI", "Helvetica Neue", Arial, sans-serif;
    font-size: 13px;
}

QMainWindow, QDialog, QWidget {
    background: #f5f6f8;
    color: #1f2328;
}

QLabel {
    background: transparent;
}

QFrame#card {
    background: #ffffff;
    border: 1px solid #e1e4e8;
    border-radius: 10px;
}

QLabel#h1 {
    font-size: 22px;
    font-weight: 600;
    color: #1f2328;
}
QLabel#h2 {
    font-size: 15px;
    font-weight: 600;
    color: #1f2328;
}
QLabel#subtitle {
    color: #6a737d;
    font-size: 13px;
}
QLabel#muted {
    color: #6a737d;
    font-size: 12px;
}
QLabel#mono {
    font-family: "JetBrains Mono", "SF Mono", "Menlo", "Consolas", monospace;
    font-size: 12px;
    color: #1f2328;
}
QLabel#formLabel {
    color: #3b424a;
    font-weight: 500;
}

QLineEdit {
    background: #ffffff;
    border: 1px solid #d0d7de;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: #c6dcff;
    color: #1f2328;
}
QLineEdit:focus {
    border: 1px solid #2d6cdf;
}
QLineEdit:disabled {
    background: #f2f3f5;
    color: #9aa2ad;
}

QPushButton {
    background: #ffffff;
    color: #1f2328;
    border: 1px solid #d0d7de;
    border-radius: 6px;
    padding: 6px 14px;
    min-height: 22px;
}
QPushButton:hover { background: #f1f3f5; }
QPushButton:pressed { background: #e5e8eb; }
QPushButton:disabled { color: #9aa2ad; background: #f5f6f8; }

QPushButton#primary {
    background: #2d6cdf;
    color: #ffffff;
    border: 1px solid #2d6cdf;
    font-weight: 600;
}
QPushButton#primary:hover  { background: #1f58c2; border-color: #1f58c2; }
QPushButton#primary:pressed{ background: #174aa6; border-color: #174aa6; }
QPushButton#primary:disabled { background: #a9c1ed; border-color: #a9c1ed; color: #ffffff; }

QPushButton#danger {
    background: #ffffff;
    color: #d63a2f;
    border: 1px solid #f0b7b2;
}
QPushButton#danger:hover  { background: #fdecea; }
QPushButton#danger:pressed{ background: #f8d5d1; }

QListWidget {
    background: #ffffff;
    border: 1px solid #e1e4e8;
    border-radius: 8px;
    padding: 4px;
    outline: 0;
}
QListWidget::item {
    padding: 2px 2px;
    border-radius: 8px;
    margin: 2px 0;
}
QListWidget::item:selected {
    background: #eaf1ff;
    color: #1f2328;
}
QListWidget::item:hover:!selected {
    background: #f5f7fb;
}

QFrame#docCard {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
}

QToolBar {
    background: #ffffff;
    border: none;
    border-bottom: 1px solid #e1e4e8;
    padding: 6px 10px;
    spacing: 6px;
}
QToolBar QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px 10px;
    color: #1f2328;
}
QToolBar QToolButton:hover {
    background: #f1f3f5;
    border-color: #d0d7de;
}
QToolBar QToolButton:pressed {
    background: #e5e8eb;
}

QStatusBar {
    background: #ffffff;
    border-top: 1px solid #e1e4e8;
}
QStatusBar::item { border: none; }
QStatusBar QLabel { padding: 2px 8px; }

QDockWidget::title {
    background: #eef0f3;
    padding: 4px 8px;
    border-bottom: 1px solid #e1e4e8;
}

QPlainTextEdit {
    background: #ffffff;
    border: none;
    color: #1f2328;
    selection-background-color: #c6dcff;
    padding: 8px 12px;
}

QMessageBox, QInputDialog {
    background: #f5f6f8;
}

QToolTip {
    background: #1f2328;
    color: #ffffff;
    border: none;
    padding: 4px 8px;
    border-radius: 4px;
}
)");
}

QColor roleColor(const QString& role) {
    if (role == QStringLiteral("owner"))  return QColor("#8e44ad");
    if (role == QStringLiteral("editor")) return QColor("#2d6cdf");
    if (role == QStringLiteral("viewer")) return QColor("#8a8f98");
    return QColor("#8a8f98");
}

QColor stateColor(const QString& state) {
    if (state == QStringLiteral("SYN")) return QColor("#2e7d32");
    if (state == QStringLiteral("ACK")) return QColor("#c07a1a");
    if (state == QStringLiteral("BUF")) return QColor("#b8622f");
    return QColor("#6a737d");
}

QString chipQss(const QColor& color) {
    return QStringLiteral(
        "background: rgba(%1, %2, %3, 38);"
        "color: %4;"
        "border-radius: 9px;"
        "padding: 2px 10px;"
        "font-size: 11px;"
        "font-weight: 600;")
        .arg(color.red()).arg(color.green()).arg(color.blue())
        .arg(color.darker(135).name());
}

QLabel* makeBadge(const QString& text, const QColor& color) {
    auto* l = new QLabel(text);
    l->setStyleSheet(QStringLiteral("QLabel { %1 }").arg(chipQss(color)));
    l->setAlignment(Qt::AlignCenter);
    return l;
}

}
