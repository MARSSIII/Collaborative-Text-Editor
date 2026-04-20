#pragma once

#include <QColor>
#include <QString>

class QLabel;

namespace collab_client::theme {

QString stylesheet();

QColor roleColor(const QString& role);
QColor stateColor(const QString& state);

QLabel* makeBadge(const QString& text, const QColor& color);
QString chipQss(const QColor& color);

}
