#pragma once

#include "collab/operation.h"

#include <QPlainTextEdit>
#include <QString>

#include <cstdint>
#include <vector>

namespace collab_client {

class EditorWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit EditorWidget(QWidget* parent = nullptr);

    void resetContent(const QString& content);

    void setIdentity(uint32_t user_id, uint32_t revision);
    void setRevision(uint32_t revision) { current_revision_ = revision; }

    void applyRemoteOperation(const collab::Operation& op);

signals:
    void localOperationsGenerated(std::vector<collab::Operation> ops);

private slots:
    void onContentsChange(int position, int charsRemoved, int charsAdded);

private:
    uint32_t user_id_ = 0;
    uint32_t current_revision_ = 0;
    bool applying_remote_ = false;
    QString last_known_text_;
};

} // namespace collab_client
