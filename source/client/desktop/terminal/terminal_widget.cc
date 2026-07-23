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

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif

#include <algorithm>

#include "proto/terminal.h"

namespace {

const char kFontResource[] = ":/fonts/JetBrainsMono-Regular.ttf";
const int kFontPointSize = 11;

// Inner padding (in pixels) between the terminal grid and the widget edges.
const int kPadding = 6;

// Maximum number of scrollback (history) lines kept above the visible screen.
const int kMaxScrollback = 5000;

// Idle delay after the last resize event before the grid is recomputed and the host is told the new
// size. While dragging the content is just stretched to follow the window.
const Milliseconds kResizeFinishDelay{ 150 };

//--------------------------------------------------------------------------------------------------
VTermModifier vtermModifiers(Qt::KeyboardModifiers modifiers)
{
    int result = VTERM_MOD_NONE;

    if (modifiers & Qt::ShiftModifier)
        result |= VTERM_MOD_SHIFT;
    if (modifiers & Qt::ControlModifier)
        result |= VTERM_MOD_CTRL;
    if (modifiers & Qt::AltModifier)
        result |= VTERM_MOD_ALT;

    return static_cast<VTermModifier>(result);
}

//--------------------------------------------------------------------------------------------------
VTermModifier modifiersFromEvent(const QKeyEvent* event)
{
    return vtermModifiers(event->modifiers());
}

//--------------------------------------------------------------------------------------------------
int vtermMouseButton(Qt::MouseButton button)
{
    switch (button)
    {
        case Qt::LeftButton:   return 1;
        case Qt::MiddleButton: return 2;
        case Qt::RightButton:  return 3;
        default:               return 0;
    }
}

//--------------------------------------------------------------------------------------------------
// Registers the bundled font once per process and returns its family name. Repeated
// addApplicationFont() calls would keep piling copies of the font data into QFontDatabase.
QString terminalFontFamily()
{
    static const QString family = []()
    {
        const int font_id = QFontDatabase::addApplicationFont(kFontResource);
        if (font_id != -1)
        {
            const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
            if (!families.isEmpty())
                return families.front();
        }
        return QString("monospace");
    }();

    return family;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent),
      default_fg_(0xD4, 0xD4, 0xD4),
      default_bg_(0x1E, 0x1E, 0x1E)
{
    selection_bg_ = palette().color(QPalette::Highlight);
    selection_fg_ = palette().color(QPalette::HighlightedText);

    font_ = QFont(terminalFontFamily(), kFontPointSize);
    font_.setStyleHint(QFont::Monospace);
    font_.setFixedPitch(true);

    QFontMetrics metrics(font_);
    char_width_ = metrics.horizontalAdvance(QChar('M'));
    line_height_ = metrics.height();
    ascent_ = metrics.ascent();

    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);

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

    blink_timer_ = new QTimer(this);
    blink_timer_->setInterval(Milliseconds(530));
    connect(blink_timer_, &QTimer::timeout, this, [this]
    {
        blink_visible_ = !blink_visible_;
        update();
    });

    bell_timer_ = new QTimer(this);
    bell_timer_->setSingleShot(true);
    bell_timer_->setInterval(Milliseconds(120));
    connect(bell_timer_, &QTimer::timeout, this, [this]
    {
        bell_flash_ = false;
        update();
    });

    resize_timer_ = new QTimer(this);
    resize_timer_->setSingleShot(true);
    connect(resize_timer_, &QTimer::timeout, this, [this]
    {
        // The snap is normally driven by the end of the border drag (WM_EXITSIZEMOVE). This timer is a
        // fallback: while the drag is still in progress, keep waiting.
        if (in_size_move_)
        {
            resize_timer_->start(kResizeFinishDelay);
            return;
        }
        finishResize();
    });

    qApp->installNativeEventFilter(this);

    vterm_ = vterm_new(rows_, columns_);
    vterm_set_utf8(vterm_, 1);

    screen_ = vterm_obtain_screen(vterm_);

    // Full-screen applications (Far, vim, mc) use the alternate screen. It must be enabled so libvterm
    // does not reflow their content on resize (the application repaints instead).
    vterm_screen_enable_altscreen(screen_, 1);

    static const VTermScreenCallbacks kCallbacks =
    {
        &TerminalWidget::onDamage,      // damage
        nullptr,                        // moverect
        &TerminalWidget::onMoveCursor,  // movecursor
        &TerminalWidget::onSetTermProp, // settermprop
        &TerminalWidget::onBell,        // bell
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
    qApp->removeNativeEventFilter(this);

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
        login_user_.clear();
        login_password_.clear();

        mode_ = Mode::LOGIN_USER;
        writeLocal("\r\n" + tr("Authentication failed.") + "\r\n");
        writeLocal(tr("User name") + ": ");
    }
}

//--------------------------------------------------------------------------------------------------
bool TerminalWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        const int key = key_event->key();

        if (key == Qt::Key_Tab || key == Qt::Key_Backtab ||
            (key >= Qt::Key_F1 && key <= Qt::Key_F35))
        {
            event->accept();
            return true;
        }
    }

    return QWidget::event(event);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // Take the keyboard focus so the in-terminal login prompt can be typed without clicking first.
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), default_bg_);

    // While a resize is in progress the grid is not recomputed yet; scale the current content to
    // follow the drag, then snap to the real grid in finishResize(). The scale is uniform on both
    // axes (by the smaller ratio) so the content keeps its proportions instead of being distorted.
    if (resizing_)
    {
        const qreal grid_width = columns_ * char_width_ + 2 * kPadding;
        const qreal grid_height = rows_ * line_height_ + 2 * kPadding;
        if (grid_width > 0 && grid_height > 0)
        {
            const qreal scale = std::min(width() / grid_width, height() / grid_height);
            painter.translate((width() - grid_width * scale) / 2.0, (height() - grid_height * scale) / 2.0);
            painter.scale(scale, scale);
        }
    }

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

            drawCell(painter, col, row, cell, isSelected(line_index, col));
        }
    }

    // The cursor is only shown on the live screen (not when scrolled into history), and hidden during
    // the dark phase of a blink while focused.
    const bool blink_hidden = hasFocus() && cursor_blink_ && !blink_visible_;
    if (scroll_offset_ == 0 && cursor_visible_ && !blink_hidden)
    {
        const int x = kPadding + cursor_col_ * char_width_;
        const int y = kPadding + cursor_row_ * line_height_;

        if (!hasFocus())
        {
            // Unfocused: outline the cell regardless of the requested shape.
            painter.setPen(default_fg_);
            painter.drawRect(x, y, char_width_ - 1, line_height_ - 1);
        }
        else if (cursor_shape_ == VTERM_PROP_CURSORSHAPE_UNDERLINE)
        {
            const int thickness = std::max(2, line_height_ / 10);
            painter.fillRect(x, y + line_height_ - thickness, char_width_, thickness, default_fg_);
        }
        else if (cursor_shape_ == VTERM_PROP_CURSORSHAPE_BAR_LEFT)
        {
            const int thickness = std::max(2, char_width_ / 6);
            painter.fillRect(x, y, thickness, line_height_, default_fg_);
        }
        else
        {
            // Block: invert the cell - fill with the foreground and repaint the glyph in the
            // background color.
            painter.fillRect(x, y, char_width_, line_height_, default_fg_);

            VTermPos pos;
            pos.row = cursor_row_;
            pos.col = cursor_col_;

            VTermScreenCell cell;
            if (vterm_screen_get_cell(screen_, pos, &cell) && cell.chars[0] != 0)
            {
                const char32_t glyph = static_cast<char32_t>(cell.chars[0]);
                painter.setFont(font_);
                painter.setPen(default_bg_);
                painter.drawText(x, y + ascent_, QString::fromUcs4(&glyph, 1));
            }
        }
    }

    // Visual bell: a short translucent flash over the whole widget (drawn in unscaled coordinates).
    if (bell_flash_)
    {
        painter.resetTransform();
        painter.fillRect(rect(), QColor(default_fg_.red(), default_fg_.green(), default_fg_.blue(), 64));
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::drawCell(
    QPainter& painter, int column, int row, const VTermScreenCell& cell, bool selected) const
{
    const int x = kPadding + column * char_width_;
    const int y = kPadding + row * line_height_;
    const int cell_width = (cell.width > 1 ? cell.width : 1) * char_width_;

    QColor fg = toColor(cell.fg, default_fg_);
    QColor bg = toColor(cell.bg, default_bg_);

    if (cell.attrs.reverse)
        std::swap(fg, bg);

    if (selected)
    {
        fg = selection_fg_;
        bg = selection_bg_;
    }

    if (bg != default_bg_ || selected)
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

    scrollbar_->setVisible(!alt_screen_ && history > 0);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::setScrollOffset(int offset)
{
    // No history scrolling while a full-screen application owns the alternate screen.
    if (alt_screen_)
        offset = 0;

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

    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
    {
        if (event->key() == Qt::Key_C)
        {
            copySelection();
            return;
        }
        if (event->key() == Qt::Key_V)
        {
            paste();
            return;
        }
    }

    // Typing returns the view to the live screen and clears the selection.
    setScrollOffset(0);
    if (has_selection_)
    {
        clearSelection();
        update();
    }

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

        case Qt::Key_Insert:
            vterm_keyboard_key(vterm_, VTERM_KEY_INS, modifiers);
            break;

        default:
        {
            // Function keys (F1..F35) carry no text and are sent as VTermKey codes.
            if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35)
            {
                const int number = event->key() - Qt::Key_F1 + 1;
                vterm_keyboard_key(vterm_, static_cast<VTermKey>(VTERM_KEY_FUNCTION(number)),
                                   modifiers);
                break;
            }

            const QString text = event->text();

            // Alt (but not AltGr, which is reported as Ctrl+Alt) turns the key into a meta sequence:
            // ESC followed by the character. While Alt is held text() is usually empty, so the
            // character is taken from the key code.
            if ((event->modifiers() & Qt::AltModifier) && !(event->modifiers() & Qt::ControlModifier))
            {
                char32_t character = 0;
                if (!text.isEmpty() && text.front().isPrint())
                    character = text.front().unicode();
                else if (event->key() >= Qt::Key_Space && event->key() <= Qt::Key_AsciiTilde)
                    character = (event->modifiers() & Qt::ShiftModifier) ?
                        static_cast<char32_t>(event->key()) : QChar(event->key()).toLower().unicode();

                if (character != 0)
                {
                    vterm_keyboard_unichar(vterm_, character, VTERM_MOD_ALT);
                    break;
                }
            }

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
    {
        // Settled back to the current grid (e.g. the transient layout resize seen while switching
        // tabs); drop any queued resize so the host is not resized - and full-screen apps not
        // repainted - for nothing.
        resize_timer_->stop();
        if (resizing_)
        {
            resizing_ = false;
            update();
        }
        return;
    }

    // Stretch the content only during an interactive window-border drag (between WM_ENTERSIZEMOVE and
    // WM_EXITSIZEMOVE). Other resizes (tab layout, maximize) do not stretch. Either way the grid change
    // is applied once the size settles (finishResize), so brief transients never reach the host.
    resizing_ = (mode_ == Mode::CONNECTED && in_size_move_);
    resize_timer_->start(kResizeFinishDelay);
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::finishResize()
{
    resizing_ = false;

    const int scrollbar_width = scrollbar_->sizeHint().width();
    const int columns = std::max(1, (width() - scrollbar_width - 2 * kPadding) / char_width_);
    const int rows = std::max(1, (height() - 2 * kPadding) / line_height_);

    if (columns != columns_ || rows != rows_)
    {
        columns_ = columns;
        rows_ = rows;

        clearSelection();

        suppress_scrollback_ = true;
        vterm_set_size(vterm_, rows_, columns_);
        suppress_scrollback_ = false;

        if (mode_ == Mode::CONNECTED)
            emit sig_resize(columns_, rows_);

        updateScrollbar();
    }

    update();
}

//--------------------------------------------------------------------------------------------------
bool TerminalWidget::nativeEventFilter(
    const QByteArray& event_type, void* message, qintptr* /* result */)
{
#if defined(Q_OS_WINDOWS)
    if (event_type == "windows_generic_MSG")
    {
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_ENTERSIZEMOVE || msg->message == WM_EXITSIZEMOVE)
        {
            // Compare only against an already created native handle. Forcing winId() here would
            // create the native window from inside the message loop; while the tab is being
            // detached (reparented) that re-enters this filter and recurses until the stack
            // overflows.
            QWidget* top = window();
            QWindow* top_handle = top ? top->windowHandle() : nullptr;

            if (top_handle && top_handle->handle() &&
                msg->hwnd == reinterpret_cast<HWND>(top_handle->winId()))
            {
                if (msg->message == WM_ENTERSIZEMOVE)
                {
                    in_size_move_ = true;
                }
                else
                {
                    in_size_move_ = false;
                    if (resizing_)
                        finishResize();
                }
            }
        }
    }
#else
    (void)event_type;
    (void)message;
#endif

    return false;
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 120;

    // Shift bypasses the application's mouse reporting and always scrolls the local history.
    const bool to_application = mouseReportingActive() && !(event->modifiers() & Qt::ShiftModifier);

    if (to_application && steps != 0)
    {
        int row = 0;
        int column = 0;
        cellAt(event->position().toPoint(), &row, &column);

        const VTermModifier modifiers = vtermModifiers(event->modifiers());
        const int button = steps > 0 ? 4 : 5;

        for (int i = 0; i < std::abs(steps); ++i)
        {
            vterm_mouse_move(vterm_, row, column, modifiers);
            vterm_mouse_button(vterm_, button, true, modifiers);
        }

        flushInput();
    }
    else if (steps != 0)
    {
        setScrollOffset(scroll_offset_ + steps * 3);
    }

    event->accept();
}

//--------------------------------------------------------------------------------------------------
bool TerminalWidget::mouseReportingActive() const
{
    return mode_ == Mode::CONNECTED && mouse_mode_ != VTERM_PROP_MOUSE_NONE && scroll_offset_ == 0;
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::cellAt(const QPoint& position, int* row, int* column) const
{
    *column = std::clamp((position.x() - kPadding) / char_width_, 0, columns_ - 1);
    *row = std::clamp((position.y() - kPadding) / line_height_, 0, rows_ - 1);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus();

    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool to_application = mouseReportingActive() && !shift;

    // Right-click opens the context menu unless the application is grabbing the mouse.
    if (event->button() == Qt::RightButton && !to_application)
    {
        showContextMenu(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    // Middle-click pastes the clipboard (when not reporting to the application).
    if (event->button() == Qt::MiddleButton && !to_application)
    {
        paste();
        event->accept();
        return;
    }

    if (to_application)
    {
        const int button = vtermMouseButton(event->button());
        if (button != 0)
        {
            int row = 0;
            int column = 0;
            cellAt(event->position().toPoint(), &row, &column);

            const VTermModifier modifiers = vtermModifiers(event->modifiers());
            vterm_mouse_move(vterm_, row, column, modifiers);
            vterm_mouse_button(vterm_, button, true, modifiers);
            flushInput();
        }

        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        const Position position = positionAt(event->position().toPoint());

        // A press shortly after a double-click is a triple-click: select the whole line.
        if (Clock::now() - last_double_click_time_ <
            Milliseconds(QApplication::doubleClickInterval()))
        {
            selectLineAt(position.line);
        }
        else
        {
            clearSelection();
            selection_anchor_ = position;
            selection_point_ = position;
            selecting_ = true;
            update();
        }

        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (selecting_)
    {
        selecting_ = false;
        event->accept();
        return;
    }

    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const int button = vtermMouseButton(event->button());

    if (!mouseReportingActive() || shift || button == 0)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    int row = 0;
    int column = 0;
    cellAt(event->position().toPoint(), &row, &column);

    const VTermModifier modifiers = vtermModifiers(event->modifiers());
    vterm_mouse_move(vterm_, row, column, modifiers);
    vterm_mouse_button(vterm_, button, false, modifiers);
    flushInput();

    event->accept();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (selecting_)
    {
        selection_point_ = positionAt(event->position().toPoint());
        has_selection_ = selection_point_.line != selection_anchor_.line ||
                         selection_point_.column != selection_anchor_.column;
        update();
        event->accept();
        return;
    }

    // CLICK mode reports presses only; DRAG mode reports moves while a button is held; MOVE mode
    // reports every motion.
    const bool report = mouseReportingActive() &&
        (mouse_mode_ == VTERM_PROP_MOUSE_MOVE ||
         (mouse_mode_ == VTERM_PROP_MOUSE_DRAG && event->buttons() != Qt::NoButton));

    if (!report)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int row = 0;
    int column = 0;
    cellAt(event->position().toPoint(), &row, &column);

    vterm_mouse_move(vterm_, row, column, vtermModifiers(event->modifiers()));
    flushInput();

    event->accept();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (event->button() == Qt::LeftButton && (!mouseReportingActive() || shift))
    {
        selectWordAt(positionAt(event->position().toPoint()));
        last_double_click_time_ = Clock::now();
        selecting_ = false;
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

//--------------------------------------------------------------------------------------------------
TerminalWidget::Position TerminalWidget::positionAt(const QPoint& position) const
{
    const int visible_row = std::clamp((position.y() - kPadding) / line_height_, 0, rows_ - 1);

    Position result;
    result.line = static_cast<int>(scrollback_.size()) - scroll_offset_ + visible_row;
    result.column = std::clamp((position.x() - kPadding) / char_width_, 0, columns_);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool TerminalWidget::cellAtLine(int line, int column, VTermScreenCell* cell) const
{
    const int history = static_cast<int>(scrollback_.size());
    if (line < history)
    {
        const std::vector<VTermScreenCell>& scrollback_line = scrollback_[line];
        if (column < 0 || column >= static_cast<int>(scrollback_line.size()))
            return false;
        *cell = scrollback_line[column];
        return true;
    }

    VTermPos pos;
    pos.row = line - history;
    pos.col = column;
    return vterm_screen_get_cell(screen_, pos, cell) != 0;
}

//--------------------------------------------------------------------------------------------------
bool TerminalWidget::isSelected(int line, int column) const
{
    if (!has_selection_)
        return false;

    Position start = selection_anchor_;
    Position end = selection_point_;
    if (start.line > end.line || (start.line == end.line && start.column > end.column))
        std::swap(start, end);

    if (line < start.line || line > end.line)
        return false;
    if (line == start.line && column < start.column)
        return false;
    if (line == end.line && column >= end.column)
        return false;
    return true;
}

//--------------------------------------------------------------------------------------------------
QString TerminalWidget::selectedText() const
{
    if (!has_selection_)
        return QString();

    Position start = selection_anchor_;
    Position end = selection_point_;
    if (start.line > end.line || (start.line == end.line && start.column > end.column))
        std::swap(start, end);

    QString result;

    for (int line = start.line; line <= end.line; ++line)
    {
        const int from = (line == start.line) ? start.column : 0;
        const int to = (line == end.line) ? end.column : columns_;

        QString text;
        for (int column = from; column < to && column < columns_; ++column)
        {
            VTermScreenCell cell;
            if (!cellAtLine(line, column, &cell))
                continue;
            if (cell.width == 0)
                continue; // Second half of a double-width character.

            if (cell.chars[0] == 0)
            {
                text += QChar(' ');
            }
            else
            {
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] != 0; ++i)
                {
                    const char32_t glyph = static_cast<char32_t>(cell.chars[i]);
                    text += QString::fromUcs4(&glyph, 1);
                }
            }
        }

        // Trailing whitespace is not part of the copied text.
        while (!text.isEmpty() && text.back() == QChar(' '))
            text.chop(1);

        result += text;
        if (line != end.line)
            result += QChar('\n');
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::selectWordAt(const Position& position)
{
    auto isWordChar = [this](int line, int column)
    {
        VTermScreenCell cell;
        if (!cellAtLine(line, column, &cell) || cell.chars[0] == 0)
            return false;
        return !QChar::isSpace(cell.chars[0]);
    };

    int left = position.column;
    int right = position.column;

    if (isWordChar(position.line, position.column))
    {
        while (left > 0 && isWordChar(position.line, left - 1))
            --left;
        while (right < columns_ - 1 && isWordChar(position.line, right + 1))
            ++right;
        ++right;
    }
    else
    {
        right = left + 1;
    }

    selection_anchor_ = { position.line, left };
    selection_point_ = { position.line, right };
    has_selection_ = true;
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::selectLineAt(int line)
{
    selection_anchor_ = { line, 0 };
    selection_point_ = { line, columns_ };
    has_selection_ = true;
    selecting_ = false;
    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::clearSelection()
{
    has_selection_ = false;
    selecting_ = false;
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::copySelection()
{
    const QString text = selectedText();
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text);
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::paste()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;

    // Terminals expect Enter as a carriage return.
    text.replace("\r\n", "\r");
    text.replace('\n', '\r');

    setScrollOffset(0);

    // Wrap the text in bracketed-paste markers when the application enabled the mode (DECSET 2004), so
    // it can tell pasted text from typed input and does not execute it line by line. start/end_paste
    // emit the markers only while the mode is active; otherwise the text is sent unchanged.
    vterm_keyboard_start_paste(vterm_);
    pending_input_.append(text.toUtf8());
    vterm_keyboard_end_paste(vterm_);
    flushInput();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::showContextMenu(const QPoint& global_position)
{
    QMenu menu(this);

    QAction* copy_action = menu.addAction(QIcon(":/img/copy.svg"), tr("Copy"));
    copy_action->setEnabled(has_selection_);

    QAction* paste_action = menu.addAction(QIcon(":/img/paste.svg"), tr("Paste"));
    paste_action->setEnabled(!QApplication::clipboard()->text().isEmpty());

    QAction* select_all_action = menu.addAction(QIcon(":/img/select-all.svg"), tr("Select All"));

    QAction* chosen = menu.exec(global_position);
    if (chosen == copy_action)
    {
        copySelection();
    }
    else if (chosen == paste_action)
    {
        paste();
    }
    else if (chosen == select_all_action)
    {
        selection_anchor_ = { 0, 0 };
        selection_point_ = { static_cast<int>(scrollback_.size()) + rows_ - 1, columns_ };
        has_selection_ = true;
        update();
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);

    blink_visible_ = true;
    if (cursor_blink_)
        blink_timer_->start();

    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);

    // No blinking while unfocused - the cursor is drawn as a hollow outline instead.
    blink_timer_->stop();

    update();
}

//--------------------------------------------------------------------------------------------------
void TerminalWidget::startLoginPrompt()
{
    mode_ = Mode::LOGIN_USER;

    writeLocal(tr("Enter your user name and password to authenticate on the remote computer."));
    writeLocal("\r\n\r\n");
    writeLocal(tr("User name") + ": ");
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
                writeLocal("\r\n" + tr("Password") + ": ");
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

    // Keep the cursor solid right after it moves (e.g. while typing) by restarting the blink phase.
    self->blink_visible_ = true;
    if (self->cursor_blink_ && self->hasFocus())
        self->blink_timer_->start();

    self->update();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onSetTermProp(VTermProp prop, VTermValue* value, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);

    if (prop == VTERM_PROP_CURSORVISIBLE)
    {
        self->cursor_visible_ = (value->boolean != 0);
        self->update();
    }
    else if (prop == VTERM_PROP_CURSORBLINK)
    {
        self->cursor_blink_ = (value->boolean != 0);
        self->blink_visible_ = true;
        if (self->cursor_blink_ && self->hasFocus())
            self->blink_timer_->start();
        else
            self->blink_timer_->stop();
        self->update();
    }
    else if (prop == VTERM_PROP_CURSORSHAPE)
    {
        self->cursor_shape_ = value->number;
        self->update();
    }
    else if (prop == VTERM_PROP_MOUSE)
    {
        self->mouse_mode_ = value->number;
    }
    else if (prop == VTERM_PROP_ALTSCREEN)
    {
        // The alternate screen (used by full-screen applications like Far, vim, mc) has no scrollback,
        // so the history view and the scrollbar are disabled while it is active.
        self->alt_screen_ = (value->boolean != 0);
        self->scroll_offset_ = 0;
        self->updateScrollbar();
        self->update();
    }

    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onBell(void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);

    QApplication::beep();

    // Briefly flash the screen so the bell is noticeable even with the system sound muted.
    self->bell_flash_ = true;
    self->bell_timer_->start();
    self->update();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onPushLine(int cols, const VTermScreenCell* cells, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);

    // A resize must not fabricate scrollback. ConPTY keeps full-screen applications (Far) on the
    // primary screen, so the spare lines libvterm scrolls off while resizing are stale frame content,
    // not history - dropping them keeps such garbage out of the scrollbar.
    if (self->suppress_scrollback_)
        return 1;

    self->scrollback_.emplace_back(cells, cells + cols);
    if (static_cast<int>(self->scrollback_.size()) > kMaxScrollback)
        self->scrollback_.pop_front();

    // Keep the viewport anchored on the same content while the user is scrolled into history.
    if (self->scroll_offset_ > 0)
    {
        self->scroll_offset_ =
            std::min(self->scroll_offset_ + 1, static_cast<int>(self->scrollback_.size()));
    }

    // Line indices shift as content scrolls, so any existing selection is dropped.
    self->clearSelection();

    self->updateScrollbar();
    return 1;
}

//--------------------------------------------------------------------------------------------------
// static
int TerminalWidget::onPopLine(int cols, VTermScreenCell* cells, void* user)
{
    TerminalWidget* self = reinterpret_cast<TerminalWidget*>(user);

    // While resizing do not pull scrollback back into the screen (it would drag stale content into the
    // top of a full-screen application); let libvterm blank-fill instead.
    if (self->suppress_scrollback_ || self->scrollback_.empty())
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
