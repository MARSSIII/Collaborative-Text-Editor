#include "client/editor_widget.h"

#include "client/cursor_translator.h"
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
    if (op.is_noop()) return;

    const auto utf8_before = last_known_text_.toUtf8();
    const auto user_cursor = textCursor();
    const uint32_t old_anchor_utf8 = utf8_codec::utf16_to_utf8_offset(
        last_known_text_, user_cursor.anchor());
    const uint32_t old_position_utf8 = utf8_codec::utf16_to_utf8_offset(
        last_known_text_, user_cursor.position());

    applying_remote_ = true;

    const int utf16_pos = utf8_codec::utf8_to_utf16_offset(utf8_before, op.position);
    QTextCursor edit_cursor(document());
    if (op.type == collab::Operation::Type::Insert) {
        edit_cursor.setPosition(utf16_pos);
        edit_cursor.insertText(QString::fromUtf8(
            op.text.data(), static_cast<int>(op.text.size())));
    } else {
        const int utf16_end = utf8_codec::utf8_to_utf16_offset(
            utf8_before, op.position + op.length);
        edit_cursor.setPosition(utf16_pos);
        edit_cursor.setPosition(utf16_end, QTextCursor::KeepAnchor);
        edit_cursor.removeSelectedText();
    }

    last_known_text_ = toPlainText();

    const uint32_t new_anchor_utf8 = cursor_translator::translate(old_anchor_utf8, op);
    const uint32_t new_position_utf8 = cursor_translator::translate(old_position_utf8, op);
    const auto utf8_after = last_known_text_.toUtf8();
    const int new_anchor_utf16 = utf8_codec::utf8_to_utf16_offset(utf8_after, new_anchor_utf8);
    const int new_position_utf16 = utf8_codec::utf8_to_utf16_offset(utf8_after, new_position_utf8);

    QTextCursor shifted(document());
    shifted.setPosition(new_anchor_utf16);
    shifted.setPosition(new_position_utf16, QTextCursor::KeepAnchor);
    setTextCursor(shifted);

    applying_remote_ = false;
}

void EditorWidget::onContentsChange(int position, int charsRemoved, int charsAdded) {
    if (applying_remote_) {
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
