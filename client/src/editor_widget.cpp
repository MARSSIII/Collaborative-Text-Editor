#include "client/editor_widget.h"

#include "client/cursor_translator.h"
#include "client/utf8_codec.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

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

void EditorWidget::setRemoteCursors(std::vector<RemoteCursor> cursors) {
    remote_cursors_ = std::move(cursors);
    viewport()->update();
}

uint32_t EditorWidget::localCursorUtf8Position() const {
    return utf8_codec::utf16_to_utf8_offset(last_known_text_, textCursor().position());
}

std::optional<std::pair<uint32_t, uint32_t>> EditorWidget::localSelectionUtf8() const {
    auto c = textCursor();
    if (!c.hasSelection()) return std::nullopt;
    const uint32_t start = utf8_codec::utf16_to_utf8_offset(last_known_text_, c.selectionStart());
    const uint32_t end = utf8_codec::utf16_to_utf8_offset(last_known_text_, c.selectionEnd());
    return std::make_pair(start, end);
}

void EditorWidget::paintEvent(QPaintEvent* event) {
    QPlainTextEdit::paintEvent(event);
    if (remote_cursors_.empty()) return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, false);
    const auto utf8 = last_known_text_.toUtf8();
    const QFontMetrics fm(font());
    const int label_height = fm.height();

    for (const auto& rc : remote_cursors_) {
        const int utf16_pos = utf8_codec::utf8_to_utf16_offset(utf8, rc.position);
        QTextCursor tc(document());
        tc.setPosition(std::clamp(utf16_pos, 0, document()->characterCount() - 1));
        const QRect caret = cursorRect(tc);
        if (!event->rect().intersects(caret.adjusted(-2, -label_height - 2, 2, 2))) {
            if (!rc.selectionStart || !rc.selectionEnd) continue;
        }

        if (rc.selectionStart && rc.selectionEnd && *rc.selectionStart != *rc.selectionEnd) {
            const uint32_t s8 = std::min(*rc.selectionStart, *rc.selectionEnd);
            const uint32_t e8 = std::max(*rc.selectionStart, *rc.selectionEnd);
            const int s16 = utf8_codec::utf8_to_utf16_offset(utf8, s8);
            const int e16 = utf8_codec::utf8_to_utf16_offset(utf8, e8);
            QTextCursor sc(document());
            sc.setPosition(s16);
            sc.setPosition(e16, QTextCursor::KeepAnchor);
            QColor fill = rc.color;
            fill.setAlpha(60);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            auto block = document()->findBlock(s16);
            int cursor_in_block = s16;
            while (block.isValid() && cursor_in_block < e16) {
                const int block_end = std::min<int>(block.position() + block.length() - 1, e16);
                QTextCursor a(document()); a.setPosition(cursor_in_block);
                QTextCursor b(document()); b.setPosition(block_end);
                QRect ra = cursorRect(a);
                QRect rb = cursorRect(b);
                QRect band(ra.left(), ra.top(), std::max(2, rb.right() - ra.left()), ra.height());
                painter.drawRect(band);
                cursor_in_block = block_end + 1;
                block = block.next();
            }
        }

        painter.setPen(QPen(rc.color, 2));
        painter.drawLine(caret.topLeft(), caret.bottomLeft());

        const QString label = rc.username;
        const int label_w = fm.horizontalAdvance(label) + 6;
        QRect label_rect(caret.left(), caret.top() - label_height - 1, label_w, label_height);
        if (label_rect.top() < 0) label_rect.moveTop(caret.bottom() + 1);
        painter.setPen(Qt::NoPen);
        painter.setBrush(rc.color);
        painter.drawRect(label_rect);
        painter.setPen(Qt::white);
        painter.drawText(label_rect.adjusted(3, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    }
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
