#pragma once

#include "collab/operation.h"

#include <QPlainTextEdit>
#include <QString>

#include <cstdint>
#include <vector>

namespace collab_client {

// Wraps QPlainTextEdit, converts its UTF-16 contentsChange deltas into
// UTF-8 Operations, and re-applies remote operations without triggering
// the feedback loop.
class EditorWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit EditorWidget(QWidget* parent = nullptr);

    // Call once, right after construction, to seed the initial document state
    // received in doc_join_response. Suppresses contentsChange emission.
    void resetContent(const QString& content);

    // Identity for Operation::userId.
    void setIdentity(uint32_t user_id, uint32_t revision);
    void setRevision(uint32_t revision) { current_revision_ = revision; }

    // Applied under the applying_remote_ guard — no localOperationsGenerated
    // will fire for the resulting contentsChange.
    void applyRemoteOperation(const collab::Operation& op);

signals:
    void localOperationsGenerated(std::vector<collab::Operation> ops);

private slots:
    void onContentsChange(int position, int charsRemoved, int charsAdded);

private:
    uint32_t user_id_ = 0;
    uint32_t current_revision_ = 0;
    bool applying_remote_ = false;
    QString last_known_text_;  // UTF-16 shadow, kept in sync after every change
};

} // namespace collab_client
