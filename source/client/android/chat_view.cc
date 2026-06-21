//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/android/chat_view.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"

namespace {

constexpr int kBubbleRadius = 14;
constexpr int kBubblePadding = 10;
constexpr int kRowSpacing = 6;
constexpr int kAutoScrollThreshold = 48;
constexpr double kMaxBubbleWidthRatio = 0.78;
constexpr double kTimeFontScale = 0.78;
constexpr double kTimeOpacity = 0.6;

constexpr int kInputRadius = 14;
constexpr int kInputMinHeight = 44;
constexpr int kInputMaxLines = 5;
constexpr int kInputDocumentMargin = 10;
constexpr double kInputOutlineWidth = 1.0;

//--------------------------------------------------------------------------------------------------
// A single chat message drawn as a rounded bubble: own messages use the accent color, incoming ones
// the surface color and carry the sender's name.
class MessageBubble final : public QWidget
{
public:
    MessageBubble(const QString& source, const QString& text, qint64 timestamp, bool outgoing,
                  QWidget* parent)
        : QWidget(parent),
          outgoing_(outgoing)
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(kBubblePadding + 2, kBubblePadding, kBubblePadding + 2, kBubblePadding);
        layout->setSpacing(2);

        const QColor accent = Controls::accentColor();
        const QColor foreground = outgoing ? Controls::contrastColor(accent)
                                           : palette().color(QPalette::WindowText);

        if (!outgoing && !source.isEmpty())
        {
            QLabel* name = new QLabel(source, this);
            QFont name_font = font();
            name_font.setWeight(QFont::Medium);
            name->setFont(name_font);

            QPalette name_palette = name->palette();
            name_palette.setColor(QPalette::WindowText, accent);
            name->setPalette(name_palette);

            layout->addWidget(name);
        }

        QLabel* body = new QLabel(text, this);
        body->setWordWrap(true);

        QPalette body_palette = body->palette();
        body_palette.setColor(QPalette::WindowText, foreground);
        body->setPalette(body_palette);

        layout->addWidget(body);

        QLabel* time = new QLabel(QDateTime::fromSecsSinceEpoch(timestamp).toString("HH:mm"), this);
        time->setFont(Controls::scaledFont(font(), kTimeFontScale));
        time->setAlignment(Qt::AlignRight);

        QColor time_color = foreground;
        time_color.setAlphaF(kTimeOpacity);
        QPalette time_palette = time->palette();
        time_palette.setColor(QPalette::WindowText, time_color);
        time->setPalette(time_palette);

        layout->addWidget(time);
    }

protected:
    //----------------------------------------------------------------------------------------------
    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(outgoing_ ? Controls::accentColor() : Controls::surfaceColor());
        painter.drawRoundedRect(rect(), kBubbleRadius, kBubbleRadius);
    }

private:
    const bool outgoing_;
};

//--------------------------------------------------------------------------------------------------
// The rounded outlined surface around the message field and the send button.
class InputBar final : public QWidget
{
public:
    explicit InputBar(QWidget* parent)
        : QWidget(parent)
    {
        // Nothing.
    }

protected:
    //----------------------------------------------------------------------------------------------
    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF surface = QRectF(rect()).adjusted(kInputOutlineWidth / 2.0, kInputOutlineWidth / 2.0,
                                                       -kInputOutlineWidth / 2.0, -kInputOutlineWidth / 2.0);
        painter.setPen(QPen(palette().color(QPalette::Mid), kInputOutlineWidth));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(surface, kInputRadius, kInputRadius);
    }
};

} // namespace

//--------------------------------------------------------------------------------------------------
ChatView::ChatView(QWidget* parent)
    : QWidget(parent),
      input_(new QTextEdit(this)),
      send_button_(new IconButton(":/img/material/send.svg", this))
{
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Kinetic finger scrolling instead of the desktop scroll bar (Qt on Android synthesizes mouse
    // events from touches).
    QScroller::grabGesture(scroll_->viewport(), QScroller::LeftMouseButtonGesture);

    // Watch the viewport for resizes (e.g. the keyboard opening) to keep following the bottom.
    scroll_->viewport()->installEventFilter(this);

    // A row appended at the bottom enlarges the scroll range only after it is laid out; follow it to
    // the new bottom then.
    connect(scroll_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this]()
    {
        if (autoscroll_)
        {
            QScrollBar* bar = scroll_->verticalScrollBar();
            bar->setValue(bar->maximum());
            autoscroll_ = false;
        }
    });

    QWidget* content = new QWidget(scroll_);
    messages_layout_ = new QVBoxLayout(content);
    messages_layout_->setContentsMargins(12, 12, 12, 12);
    messages_layout_->setSpacing(kRowSpacing);
    messages_layout_->addStretch();
    scroll_->setWidget(content);

    // A borderless multi-line field on the bar's own surface; it grows with its content.
    input_->setFont(Controls::scaledFont(font(), Controls::kFontScale));
    input_->setFrameShape(QFrame::NoFrame);
    input_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input_->setAcceptRichText(false);
    input_->setPlaceholderText(tr("Message"));
    // Suppress the draggable teardrop text handles Android draws under the cursor.
    input_->setInputMethodHints(input_->inputMethodHints() | Qt::ImhNoTextHandles);
    // A symmetric document margin pads the field to a comfortable touch height and keeps a single
    // line vertically centered (top margin equals bottom margin).
    input_->document()->setDocumentMargin(kInputDocumentMargin);
    input_->installEventFilter(this);

    QPalette input_palette = input_->palette();
    input_palette.setColor(QPalette::Base, Qt::transparent);
    QColor placeholder_color = input_palette.color(QPalette::WindowText);
    placeholder_color.setAlphaF(0.5);
    input_palette.setColor(QPalette::PlaceholderText, placeholder_color);
    input_->setPalette(input_palette);

    // Finger scrolling for the overflowing content (no desktop scroll bar).
    QScroller::grabGesture(input_->viewport(), QScroller::LeftMouseButtonGesture);

    connect(input_, &QTextEdit::textChanged, this, &ChatView::updateInputHeight);
    connect(input_, &QTextEdit::textChanged, this, [this]()
    {
        // Skip the programmatic clear after sending; only a real edit means the user is typing.
        if (!input_->document()->isEmpty())
            emit sig_typing();
    });

    InputBar* input_bar = new InputBar(this);
    QHBoxLayout* bar_layout = new QHBoxLayout(input_bar);
    bar_layout->setContentsMargins(6, 2, 4, 2);
    bar_layout->setSpacing(0);
    bar_layout->addWidget(input_, 1);
    bar_layout->addWidget(send_button_, 0, Qt::AlignBottom);

    QHBoxLayout* input_row = new QHBoxLayout();
    input_row->setContentsMargins(8, 4, 8, 12);
    input_row->addWidget(input_bar);

    status_ = new Label(QString(), Label::Role::CAPTION, this);
    status_->setAlignment(Qt::AlignCenter);
    // Reserve one line so the status text appears and disappears without shifting the messages.
    status_->setMinimumHeight(QFontMetrics(status_->font()).height());

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll_, 1);
    layout->addWidget(status_);
    layout->addLayout(input_row);

    // The button must not take focus from the field: otherwise the first tap only moves focus and
    // hides the keyboard instead of sending.
    send_button_->setFocusPolicy(Qt::NoFocus);

    connect(send_button_, &IconButton::clicked, this, &ChatView::onSend);

    updateInputHeight();
    setInputEnabled(false);
}

//--------------------------------------------------------------------------------------------------
ChatView::~ChatView() = default;

//--------------------------------------------------------------------------------------------------
void ChatView::addMessage(const QString& source, const QString& text, bool outgoing, qint64 timestamp)
{
    QWidget* row = new QWidget();
    QHBoxLayout* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);

    MessageBubble* bubble = new MessageBubble(source, text, timestamp, outgoing, row);
    bubble->setMaximumWidth(static_cast<int>(width() * kMaxBubbleWidthRatio));
    bubbles_.append(bubble);

    if (outgoing)
    {
        row_layout->addStretch();
        row_layout->addWidget(bubble);
    }
    else
    {
        row_layout->addWidget(bubble);
        row_layout->addStretch();
    }

    appendRow(row);
}

//--------------------------------------------------------------------------------------------------
void ChatView::addStatus(const QString& text)
{
    Label* status = new Label(text, Label::Role::CAPTION);
    status->setAlignment(Qt::AlignCenter);
    status->setWordWrap(true);

    QWidget* row = new QWidget();
    QHBoxLayout* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->addWidget(status);

    appendRow(row);
}

//--------------------------------------------------------------------------------------------------
void ChatView::clear()
{
    bubbles_.clear();

    // Remove every row, keeping the trailing stretch.
    while (messages_layout_->count() > 1)
    {
        QLayoutItem* item = messages_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

//--------------------------------------------------------------------------------------------------
void ChatView::setStatusText(const QString& text)
{
    status_->setText(text);
}

//--------------------------------------------------------------------------------------------------
void ChatView::setInputEnabled(bool enabled)
{
    input_->setEnabled(enabled);
    send_button_->setEnabled(enabled);
}

//--------------------------------------------------------------------------------------------------
bool ChatView::eventFilter(QObject* watched, QEvent* event)
{
    // Enter sends the message; Shift+Enter inserts a line break.
    if (watched == input_ && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        if ((key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) &&
            !(key_event->modifiers() & Qt::ShiftModifier))
        {
            onSend();
            return true;
        }
    }

    // When the viewport shrinks (the keyboard opens), keep following the bottom if we were there;
    // the resize changes the scroll range, and the rangeChanged handler then scrolls to it.
    if (watched == scroll_->viewport() && event->type() == QEvent::Resize)
    {
        QScrollBar* bar = scroll_->verticalScrollBar();
        if (bar->value() >= bar->maximum() - kAutoScrollThreshold)
            autoscroll_ = true;
    }

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void ChatView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Re-clamp every bubble to the current width. This also sizes bubbles that were added before the
    // view had a width (history loaded at startup).
    const int max_width = static_cast<int>(width() * kMaxBubbleWidthRatio);
    for (QWidget* bubble : std::as_const(bubbles_))
        bubble->setMaximumWidth(max_width);
}

//--------------------------------------------------------------------------------------------------
void ChatView::onSend()
{
    const QString text = input_->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    emit sig_sendText(text);
    input_->clear();
}

//--------------------------------------------------------------------------------------------------
void ChatView::updateInputHeight()
{
    // The field hugs its content, growing with each line up to kInputMaxLines, then scrolls. The
    // maximum is derived from the rendered line height (document()->size() over-estimates it, which
    // let an extra line peek in).
    const int line_spacing = QFontMetrics(input_->font()).lineSpacing();
    const int margins = 2 * static_cast<int>(input_->document()->documentMargin());
    const int max_height = kInputMaxLines * line_spacing + margins;

    const int content_height = static_cast<int>(input_->document()->size().height());
    input_->setFixedHeight(std::clamp(content_height, kInputMinHeight, max_height));

    // setFixedHeight resets the scroll to the top. While typing the caret is at the end, so scroll
    // all the way down to show the last line with its bottom margin.
    QScrollBar* bar = input_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

//--------------------------------------------------------------------------------------------------
void ChatView::appendRow(QWidget* row)
{
    const bool input_had_focus = input_->hasFocus();

    // Follow the new row only if the list is already at the bottom, so reading earlier history is
    // not interrupted.
    QScrollBar* bar = scroll_->verticalScrollBar();
    autoscroll_ = bar->value() >= bar->maximum() - kAutoScrollThreshold;

    // Insert before the trailing stretch so messages stack from the top.
    messages_layout_->insertWidget(messages_layout_->count() - 1, row);

    // Adding a row must not pull focus off the input, which would drop the keyboard and leave the
    // window in a half-resized state on the next tap.
    if (input_had_focus && !input_->hasFocus())
        input_->setFocus();
}
