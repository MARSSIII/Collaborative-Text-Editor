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
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace collab_client {

EditorWidget::EditorWidget(QWidget* parent) : QPlainTextEdit(parent) {
    connect(document(), &QTextDocument::contentsChange,
            this, &EditorWidget::onContentsChange);
    cursor_anim_timer_ = new QTimer(this);
    cursor_anim_timer_->setInterval(16);
    connect(cursor_anim_timer_, &QTimer::timeout,
            this, &EditorWidget::tickCursorAnimation);
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
    std::unordered_map<uint32_t, CursorAnim> next;
    next.reserve(remote_cursors_.size());
    for (const auto& rc : remote_cursors_) {
        auto it = cursor_anim_.find(rc.userId);

        if (it == cursor_anim_.end()) {
            CursorAnim a;
            a.display_position = static_cast<double>(rc.position);

            if (rc.selectionStart) a.display_sel_start = static_cast<double>(*rc.selectionStart);
            if (rc.selectionEnd) a.display_sel_end = static_cast<double>(*rc.selectionEnd);

            next.emplace(rc.userId, a);
        } else {
            next.emplace(rc.userId, it->second);
        }
    }
    cursor_anim_ = std::move(next);
    if (!cursor_anim_timer_->isActive() && !remote_cursors_.empty()) cursor_anim_timer_->start();
    viewport()->update();
}

void EditorWidget::tickCursorAnimation() {
    if (remote_cursors_.empty()) { cursor_anim_timer_->stop(); return; }
    constexpr double kEase = 0.28;
    constexpr double kSnap = 0.5;
    bool any_moving = false;

    auto step = [&](double& disp, double target) {
        const double diff = target - disp;
        if (std::abs(diff) < kSnap) { disp = target; return; }
        disp += diff * kEase;
        any_moving = true;
    };

    for (const auto& rc : remote_cursors_) {
        auto& a = cursor_anim_[rc.userId];
        step(a.display_position, static_cast<double>(rc.position));

        if (rc.selectionStart) {
            if (!a.display_sel_start) a.display_sel_start = static_cast<double>(*rc.selectionStart);
            step(*a.display_sel_start, static_cast<double>(*rc.selectionStart));
        } else {
            a.display_sel_start.reset();
        }

        if (rc.selectionEnd) {
            if (!a.display_sel_end) a.display_sel_end = static_cast<double>(*rc.selectionEnd);
            step(*a.display_sel_end, static_cast<double>(*rc.selectionEnd));
        } else {
            a.display_sel_end.reset();
        }
    }

    viewport()->update();
    if (!any_moving) cursor_anim_timer_->stop();
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
        const auto disp = displayedPositionsFor(rc);

        const int utf16_pos = utf8_codec::utf8_to_utf16_offset(utf8, disp.position);
        QTextCursor tc(document());
        tc.setPosition(std::clamp(utf16_pos, 0, document()->characterCount() - 1));
        const QRect caret = cursorRect(tc);

        const bool caret_visible = event->rect().intersects(
            caret.adjusted(-2, -label_height - 2, 2, 2));
        const bool has_selection = disp.selectionStart && disp.selectionEnd
            && *disp.selectionStart != *disp.selectionEnd;
        if (!caret_visible && !has_selection) continue;

        if (has_selection) {
            const uint32_t s8 = std::min(*disp.selectionStart, *disp.selectionEnd);
            const uint32_t e8 = std::max(*disp.selectionStart, *disp.selectionEnd);
            paintSelectionBand(painter, utf8, s8, e8, rc.color);
        }
        if (caret_visible) paintCaretAndLabel(painter, caret, rc, fm);
    }
}

EditorWidget::DisplayedPositions
EditorWidget::displayedPositionsFor(const RemoteCursor& rc) const {
    auto it = cursor_anim_.find(rc.userId);
    const bool animated = it != cursor_anim_.end();

    auto pick = [&](double anim_val, uint32_t fallback) {
        return animated ? static_cast<uint32_t>(std::lround(anim_val)) : fallback;
    };

    DisplayedPositions out;
    out.position = pick(animated ? it->second.display_position : 0.0, rc.position);
    if (rc.selectionStart) {
        const bool a = animated && it->second.display_sel_start.has_value();
        out.selectionStart = a ? static_cast<uint32_t>(std::lround(*it->second.display_sel_start))
                               : *rc.selectionStart;
    }
    if (rc.selectionEnd) {
        const bool a = animated && it->second.display_sel_end.has_value();
        out.selectionEnd = a ? static_cast<uint32_t>(std::lround(*it->second.display_sel_end))
                             : *rc.selectionEnd;
    }
    return out;
}

void EditorWidget::paintSelectionBand(QPainter& painter, const QByteArray& utf8,
                                      uint32_t start_utf8, uint32_t end_utf8,
                                      const QColor& color) {
    const int s16 = utf8_codec::utf8_to_utf16_offset(utf8, start_utf8);
    const int e16 = utf8_codec::utf8_to_utf16_offset(utf8, end_utf8);

    QColor fill = color;
    fill.setAlpha(60);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);

    auto block = document()->findBlock(s16);
    int cursor_in_block = s16;
    while (block.isValid() && cursor_in_block < e16) {
        const int block_end = std::min<int>(block.position() + block.length() - 1, e16);
        QTextCursor a(document()); a.setPosition(cursor_in_block);
        QTextCursor b(document()); b.setPosition(block_end);
        const QRect ra = cursorRect(a);
        const QRect rb = cursorRect(b);
        const QRect band(ra.left(), ra.top(),
                         std::max(2, rb.right() - ra.left()), ra.height());
        painter.drawRect(band);
        cursor_in_block = block_end + 1;
        block = block.next();
    }
}

void EditorWidget::paintCaretAndLabel(QPainter& painter, const QRect& caret,
                                      const RemoteCursor& rc,
                                      const QFontMetrics& fm) {
    painter.setPen(QPen(rc.color, 2));
    painter.drawLine(caret.topLeft(), caret.bottomLeft());

    const int label_height = fm.height();
    const int label_w = fm.horizontalAdvance(rc.username) + 6;
    QRect label_rect(caret.left(), caret.top() - label_height - 1, label_w, label_height);
    if (label_rect.top() < 0) label_rect.moveTop(caret.bottom() + 1);

    painter.setPen(Qt::NoPen);
    painter.setBrush(rc.color);
    painter.drawRect(label_rect);
    painter.setPen(Qt::white);
    painter.drawText(label_rect.adjusted(3, 0, -3, 0),
                     Qt::AlignVCenter | Qt::AlignLeft, rc.username);
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

}
