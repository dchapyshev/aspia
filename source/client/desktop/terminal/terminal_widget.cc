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

#include "client/desktop/terminal/terminal_widget.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

#include "base/logging.h"
#include "proto/terminal.h"

namespace {

const char kFontResource[] = ":/fonts/JetBrainsMono-Regular.ttf";
const int kFontPointSize = 11;

// Inner padding (in pixels) between the terminal grid and the widget edges.
const int kPadding = 6;

// Maximum number of scrollback (history) lines kept above the visible screen.
const int kMaxScrollback = 5000;

//--------------------------------------------------------------------------------------------------
VTermModifier modifiersFromEvent(const QKeyEvent* event)
{
    int modifiers = VTERM_MOD_NONE;

    if (event->modifiers() & Qt::ShiftModifier)
        modifiers |= VTERM_MOD_SHIFT;
    if (event->modifiers() & Qt::ControlModifier)
        modifiers |= VTERM_MOD_CTRL;
    if (event->modifiers() & Qt::AltModifier)
        modifiers |= VTERM_MOD_ALT;

    return static_cast<VTermModifier>(modifiers);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent),
      default_fg_(0xD4, 0xD4, 0xD4),
      default_bg_(0x1E, 0x1E, 0x1E)
{
    int font_id = QFontDatabase::addApplicationFont(kFontResource);
    QString family = "monospace";
    if (font_id != -1)
    {
        const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
        if (!families.isEmpty())
            family = families.front();
    }

    font_ = QFont(family, kFontPointSize);
    font_.setStyleHint(QFont::Monospace);
    font_.setFixedPitch(true);

    QFontMetrics metrics(font_);
    char_width_ = metrics.horizontalAdvance(QChar('M'));
    line_height_ = metrics.height();
    ascent_ = metrics.ascent();

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, default_bg_);
    setPalette(pal);
    setAutoFillBackground(true);

    scrollbar_ = new QScrollBar(Qt::Vertical, this);
    scrollbar_->setVisible(false);
    connect(scrollbar_, &QScrollBar::valueChanged, this, [this](int value)
    {
        setScrollOffset(scrollbar_->maximum() - value);
    });

    vterm_ = vterm_new(rows_, columns_);
    vterm_set_utf8(vterm_, 1);

    screen_ = vterm_obtain_screen(vterm_);

    static const VTermScreenCallbacks kCallbacks =
    {
        &TerminalWidget::onDamage,      // damage
        nullptr,                        // moverect
        &TerminalWidget::onMoveCursor,  // movecursor
        &TerminalWidget::onSetTermProp, // settermprop
        nullptr,                        // bell
        nullptr,                        // resize
        &TerminalWidget::onPushLine,    // sb_pushline
        &TerminalWidget::onPopLine,     // sb_popline
        nullptr                         // sb_clear
    };

    vterm_screen_set_callbacks(screen_, &kCallbacks, this);
    vterm_screen_reset(screen_, 1);
    vterm_output_set_callback(vterm_, &TerminalWidget::onOutput, this);

    startLoginPrompt();
}

//--------------------------------------------------------------------------------------------------
TerminalWidget::~TerminalWidget()
{
    if (vterm_)
        vterm_free(vterm_);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::writeOutput(const QByteArray& data)
{
    vterm_input_write(vterm_, data.constData(), data.size());

    // libvterm may emit replies (e.g. device attribute or cursor position reports) while processing
    // the host output; forward them back to the host.
    if (mode_ == Mode::CONNECTED)
        flushInput();
    else
        pending_input_.clear();

    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::onResult(int result_code)
{
    if (result_code == proto::terminal::Result::CODE_AUTHENTICATION_ERROR)
    {
        writeLocal("\r\nAuthentication failed.\r\n");
        login_user_.clear();
        login_password_.clear();
        startLoginPrompt();
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), default_bg_);
    painter.setFont(font_);

    const int history = static_cast<int>(scrollback_.size());

    for (int row = 0; row < rows_; ++row)
    {
        // Index in the combined (scrollback + live screen) line stream.
        const int line_index = history - scroll_offset_ + row;

        for (int col = 0; col < columns_; ++col)
        {
            VTermScreenCell cell;

            if (line_index < history)
            {
                const std::vector<VTermScreenCell>& line = scrollback_[line_index];
                if (col >= static_cast<int>(line.size()))
                    continue;
                cell = line[col];
            }
            else
            {
                VTermPos pos;
                pos.row = line_index - history;
                pos.col = col;
                if (!vterm_screen_get_cell(screen_, pos, &cell))
                    continue;
            }

            drawCell(painter, col, row, cell);
        }
    }

    // The cursor is only shown on the live screen (not when scrolled into history).
    if (scroll_offset_ == 0 && cursor_visible_)
    {
        const int x = kPadding + cursor_col_ * char_width_;
        const int y = kPadding + cursor_row_ * line_height_;

        if (hasFocus())
        {
            painter.fillRect(x, y, char_width_, line_height_, default_fg_);

            VTermPos pos;
            pos.row = cursor_row_;
            pos.col = cursor_col_;

            VTermScreenCell cell;
            if (vterm_screen_get_cell(screen_, pos, &cell) && cell.chars[0] != 0)
            {
                const char32_t glyph = static_cast<char32_t>(cell.chars[0]);
                painter.setPen(default_bg_);
                painter.drawText(x, y + ascent_, QString::fromUcs4(&glyph, 1));
            }
        }
        else
        {
            painter.setPen(default_fg_);
            painter.drawRect(x, y, char_width_ - 1, line_height_ - 1);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::drawCell(QPainter& painter, int column, int row, const VTermScreenCell& cell) const
{
    const int x = kPadding + column * char_width_;
    const int y = kPadding + row * line_height_;
    const int cell_width = (cell.width > 1 ? cell.width : 1) * char_width_;

    QColor fg = toColor(cell.fg, default_fg_);
    QColor bg = toColor(cell.bg, default_bg_);

    if (cell.attrs.reverse)
        std::swap(fg, bg);

    if (bg != default_bg_)
        painter.fillRect(x, y, cell_width, line_height_, bg);

    if (cell.chars[0] != 0)
    {
        QFont cell_font = font_;
        if (cell.attrs.bold)
            cell_font.setBold(true);
        if (cell.attrs.italic)
            cell_font.setItalic(true);
        if (cell.attrs.underline != VTERM_UNDERLINE_OFF)
            cell_font.setUnderline(true);
        painter.setFont(cell_font);

        const char32_t glyph = static_cast<char32_t>(cell.chars[0]);
        painter.setPen(fg);
        painter.drawText(x, y + ascent_, QString::fromUcs4(&glyph, 1));
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::updateScrollbar()
{
    const int history = static_cast<int>(scrollback_.size());

    scrollbar_->blockSignals(true);
    scrollbar_->setRange(0, history);
    scrollbar_->setPageStep(rows_);
    scrollbar_->setValue(history - scroll_offset_);
    scrollbar_->blockSignals(false);

    scrollbar_->setVisible(history > 0);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::setScrollOffset(int offset)
{
    offset = std::clamp(offset, 0, static_cast<int>(scrollback_.size()));
    if (offset == scroll_offset_)
        return;

    scroll_offset_ = offset;
    updateScrollbar();
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::keyPressEvent(QKeyEvent* event)
{
    if (mode_ != Mode::CONNECTED)
    {
        handleLoginKey(event);
        return;
    }

    // Typing returns the view to the live screen.
    setScrollOffset(0);

    const VTermModifier modifiers = modifiersFromEvent(event);

    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            vterm_keyboard_key(vterm_, VTERM_KEY_ENTER, modifiers);
            break;

        case Qt::Key_Backspace:
            vterm_keyboard_key(vterm_, VTERM_KEY_BACKSPACE, modifiers);
            break;

        case Qt::Key_Tab:
            vterm_keyboard_key(vterm_, VTERM_KEY_TAB, modifiers);
            break;

        case Qt::Key_Escape:
            vterm_keyboard_key(vterm_, VTERM_KEY_ESCAPE, modifiers);
            break;

        case Qt::Key_Up:
            vterm_keyboard_key(vterm_, VTERM_KEY_UP, modifiers);
            break;

        case Qt::Key_Down:
            vterm_keyboard_key(vterm_, VTERM_KEY_DOWN, modifiers);
            break;

        case Qt::Key_Left:
            vterm_keyboard_key(vterm_, VTERM_KEY_LEFT, modifiers);
            break;

        case Qt::Key_Right:
            vterm_keyboard_key(vterm_, VTERM_KEY_RIGHT, modifiers);
            break;

        case Qt::Key_Home:
            vterm_keyboard_key(vterm_, VTERM_KEY_HOME, modifiers);
            break;

        case Qt::Key_End:
            vterm_keyboard_key(vterm_, VTERM_KEY_END, modifiers);
            break;

        case Qt::Key_PageUp:
            vterm_keyboard_key(vterm_, VTERM_KEY_PAGEUP, modifiers);
            break;

        case Qt::Key_PageDown:
            vterm_keyboard_key(vterm_, VTERM_KEY_PAGEDOWN, modifiers);
            break;

        case Qt::Key_Delete:
            vterm_keyboard_key(vterm_, VTERM_KEY_DEL, modifiers);
            break;

        default:
        {
            const QString text = event->text();
            if (text.isEmpty())
            {
                QWidget::keyPressEvent(event);
                return;
            }

            for (const QChar& character : text)
                vterm_keyboard_unichar(vterm_, character.unicode(), VTERM_MOD_NONE);
        }
        break;
    }

    flushInput();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::resizeEvent(QResizeEvent* /* event */)
{
    const int scrollbar_width = scrollbar_->sizeHint().width();
    scrollbar_->setGeometry(width() - scrollbar_width, 0, scrollbar_width, height());

    if (char_width_ <= 0 || line_height_ <= 0)
        return;

    const int columns = std::max(1, (width() - scrollbar_width - 2 * kPadding) / char_width_);
    const int rows = std::max(1, (height() - 2 * kPadding) / line_height_);

    if (columns == columns_ && rows == rows_)
        return;

    columns_ = columns;
    rows_ = rows;

    vterm_set_size(vterm_, rows_, columns_);

    if (mode_ == Mode::CONNECTED)
        emit sig_resize(columns_, rows_);

    updateScrollbar();
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0)
        setScrollOffset(scroll_offset_ + steps * 3);

    event->accept();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::startLoginPrompt()
{
    mode_ = Mode::LOGIN_USER;
    writeLocal("login: ");
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::handleLoginKey(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (mode_ == Mode::LOGIN_USER)
            {
                writeLocal("\r\npassword: ");
                mode_ = Mode::LOGIN_PASSWORD;
            }
            else
            {
                writeLocal("\r\n");
                mode_ = Mode::CONNECTED;
                emit sig_credentials(login_user_, login_password_);

                // Make sure the host pseudo-terminal matches the current widget size.
                emit sig_resize(columns_, rows_);
            }
            return;

        case Qt::Key_Backspace:
            if (mode_ == Mode::LOGIN_USER && !login_user_.isEmpty())
            {
                login_user_.chop(1);
                writeLocal("\b \b");
            }
            else if (mode_ == Mode::LOGIN_PASSWORD && !login_password_.isEmpty())
            {
                login_password_.chop(1);
            }
            return;

        default:
            break;
    }

    const QString text = event->text();
    if (text.isEmpty() || !text.front().isPrint())
        return;

    if (mode_ == Mode::LOGIN_USER)
    {
        login_user_ += text;
        writeLocal(text);
    }
    else
    {
        login_password_ += text;
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::writeLocal(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    vterm_input_write(vterm_, utf8.constData(), utf8.size());

    // Local prompt text never produces input for the host.
    pending_input_.clear();
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::flushInput()
{
    if (pending_input_.isEmpty())
        return;

    emit sig_input(pending_input_);
    pending_input_.clear();
}

//--------------------------------------------------------------------------------------------------
QColor TerminalWidget::toColor(const VTermColor& color, const QColor& fallback) const
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&color) || VTERM_COLOR_IS_DEFAULT_BG(&color))
        return fallback;

    VTermColor rgb = color;
    if (VTERM_COLOR_IS_INDEXED(&rgb))
        vterm_screen_convert_color_to_rgb(screen_, &rgb);

    return QColor(rgb.rgb.red, rgb.rgb.green, rgb.rgb.blue);
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onDamage(VTermRect /* rect */, void* user)
{
    reinterpret_cast<TerminalWidget*>(user)->update();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onMoveCursor(VTermPos pos, VTermPos /* old_pos */, int visible, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);
    self->cursor_row_ = pos.row;
    self->cursor_col_ = pos.col;
    self->cursor_visible_ = (visible != 0);
    self->update();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onSetTermProp(VTermProp prop, VTermValue* value, void* user)
{
    if (prop == VTERM_PROP_CURSORVISIBLE)
    {
        TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);
        self->cursor_visible_ = (value->boolean != 0);
        self->update();
    }

    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onPushLine(int cols, const VTermScreenCell* cells, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);

    self->scrollback_.emplace_back(cells, cells + cols);
    if (static_cast<int>(self->scrollback_.size()) > kMaxScrollback)
        self->scrollback_.pop_front();

    // Keep the viewport anchored on the same content while the user is scrolled into history.
    if (self->scroll_offset_ > 0)
    {
        self->scroll_offset_ =
            std::min(self->scroll_offset_ + 1, static_cast<int>(self->scrollback_.size()));
    }

    self->updateScrollbar();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onPopLine(int cols, VTermScreenCell* cells, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);
    if (self->scrollback_.empty())
        return 0;

    const std::vector<VTermScreenCell>& line = self->scrollback_.back();

    VTermScreenCell blank = {};
    blank.width = 1;

    for (int i = 0; i < cols; ++i)
        cells[i] = (i < static_cast<int>(line.size())) ? line[i] : blank;

    self->scrollback_.pop_back();

    if (self->scroll_offset_ > static_cast<int>(self->scrollback_.size()))
        self->scroll_offset_ = static_cast<int>(self->scrollback_.size());

    self->updateScrollbar();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
void TerminalWidget::onOutput(const char* bytes, size_t length, void* user)
{
    reinterpret_cast<TerminalWidget*>(user)->pending_input_.append(
        bytes, static_cast<int>(length));
}
