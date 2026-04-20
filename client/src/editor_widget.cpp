#include "client/editor_widget.h"

#include "client/utf8_codec.h"

#include <QTextCursor>
#include <QTextDocument>

namespace collab_client {

EditorWidget::EditorWidget(QWidget* parent) : QPlainTextEdit(parent) {
    connect(document(), &QTextDocument::contentsChange,
            this, &EditorWidget::onContentsChange);
}

void EditorWidget::resetContent(const QString& content) {
    applying_remote_ = true;
    setPlainText(content);
    last_known_text_ = content;
    applying_remote_ = false;
}

void EditorWidget::setIdentity(uint32_t user_id, uint32_t revision) {
    user_id_ = user_id;
    current_revision_ = revision;
}

void EditorWidget::applyRemoteOperation(const collab::Operation& op) {
    applying_remote_ = true;

    const auto utf8_current = last_known_text_.toUtf8();
    const int utf16_pos = utf8_codec::utf8_to_utf16_offset(utf8_current, op.position);

    QTextCursor c(document());
    if (op.type == collab::Operation::Type::Insert) {
        c.setPosition(utf16_pos);
        c.insertText(QString::fromUtf8(op.text.data(),
                                       static_cast<int>(op.text.size())));
    } else {
        const int utf16_end = utf8_codec::utf8_to_utf16_offset(
            utf8_current, op.position + op.length);
        c.setPosition(utf16_pos);
        c.setPosition(utf16_end, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    }

    last_known_text_ = toPlainText();
    applying_remote_ = false;
}

void EditorWidget::onContentsChange(int position, int charsRemoved, int charsAdded) {
    if (applying_remote_) {
        // Keep the shadow in sync even for self-initiated programmatic edits.
        last_known_text_ = toPlainText();
        return;
    }

    const uint32_t utf8_pos = utf8_codec::utf16_to_utf8_offset(last_known_text_,
                                                               position);

    std::vector<collab::Operation> ops;
    ops.reserve(2);

    if (charsRemoved > 0) {
        const QString removed_utf16 = last_known_text_.mid(position, charsRemoved);
        const auto removed_utf8 = removed_utf16.toUtf8();
        ops.push_back(collab::make_delete(
            utf8_pos,
            static_cast<uint32_t>(removed_utf8.size()),
            std::string(removed_utf8.constData(),
                        static_cast<size_t>(removed_utf8.size())),
            user_id_,
            current_revision_));
    }

    if (charsAdded > 0) {
        const QString added_utf16 = toPlainText().mid(position, charsAdded);
        const auto added_utf8 = added_utf16.toUtf8();
        ops.push_back(collab::make_insert(
            utf8_pos,
            std::string(added_utf8.constData(),
                        static_cast<size_t>(added_utf8.size())),
            user_id_,
            current_revision_));
    }

    last_known_text_ = toPlainText();

    if (!ops.empty()) emit localOperationsGenerated(std::move(ops));
}

} // namespace collab_client
