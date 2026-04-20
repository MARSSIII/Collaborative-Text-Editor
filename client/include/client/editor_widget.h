#pragma once

#include "client/remote_cursors_model.h"
#include "collab/operation.h"

#include <QPlainTextEdit>
#include <QString>

#include <cstdint>
#include <optional>
#include <utility>
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
    void setRemoteCursors(std::vector<RemoteCursor> cursors);

    uint32_t localCursorUtf8Position() const;
    std::optional<std::pair<uint32_t, uint32_t>> localSelectionUtf8() const;

signals:
    void localOperationsGenerated(std::vector<collab::Operation> ops);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onContentsChange(int position, int charsRemoved, int charsAdded);

private:
    uint32_t user_id_ = 0;
    uint32_t current_revision_ = 0;
    bool applying_remote_ = false;
    QString last_known_text_;
    std::vector<RemoteCursor> remote_cursors_;
};

} // namespace collab_client
