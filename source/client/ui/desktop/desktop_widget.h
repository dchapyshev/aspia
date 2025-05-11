//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_DESKTOP_WIDGET_H
#define CLIENT_UI_DESKTOP_DESKTOP_WIDGET_H

#include "base/desktop/frame.h"
#include "proto/desktop.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_user_object.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
#include "base/mac/scoped_cftyperef.h"
#include <CoreGraphics/CGEventTypes.h>
#endif // defined(Q_OS_MACOS)

#include <QEvent>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <set>

namespace client {

class DesktopWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopWidget(QWidget* parent);
    ~DesktopWidget() final;

    base::Frame* desktopFrame();
    void setDesktopFrame(std::shared_ptr<base::Frame> frame);
    void setDesktopFrameError(proto::VideoErrorCode error_code);
    void drawDesktopFrame();
    void setCursorShape(QPixmap&& cursor_shape, const QPoint& hotspot);
    void setCursorPosition(const QPoint& cursor_position);

    void doMouseEvent(QEvent::Type event_type,
                      const Qt::MouseButtons& buttons,
                      const QPoint& pos,
                      const QPoint& delta = QPoint());
    void doKeyEvent(QKeyEvent* event);

public slots:
    void executeKeyCombination(int key_sequence);

    // Enables or disables the sending of key combinations. It only affects the input received
    // from the user. Slot |executeKeySequense| can send key combinations.
    void enableKeyCombinations(bool enable);
    void enableRemoteCursorPosition(bool enable);
    void userLeftFromWindow();

signals:
    void sig_mouseEvent(const proto::MouseEvent& event);
    void sig_keyEvent(const proto::KeyEvent& event);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;
    void mouseDoubleClickEvent(QMouseEvent* event) final;
    void keyPressEvent(QKeyEvent* event) final;
    void keyReleaseEvent(QKeyEvent* event) final;
    void leaveEvent(QEvent *event) final;
    void focusInEvent(QFocusEvent* event) final;
    void focusOutEvent(QFocusEvent* event) final;

private:
    void executeKeyEvent(quint32 usb_keycode, quint32 flags);
    void enableKeyHooks(bool enable);
    void releaseMouseButtons();
    void releaseKeyboardButtons();

    QPainter painter_;

#if defined(Q_OS_WINDOWS)
    static LRESULT CALLBACK keyboardHookProc(INT code, WPARAM wparam, LPARAM lparam);
    base::ScopedHHOOK keyboard_hook_;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
    static CGEventRef keyboardFilterProc(
        CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* user_info);
    base::ScopedCFTypeRef<CFMachPortRef> event_tap_;
#endif // defined(Q_OS_MACOS)

    QPointer<QTimer> error_timer_;
    proto::VideoErrorCode last_error_code_ = proto::VIDEO_ERROR_CODE_OK;
    proto::VideoErrorCode current_error_code_ = proto::VIDEO_ERROR_CODE_OK;
    std::unique_ptr<QImage> error_image_;

    std::shared_ptr<base::Frame> frame_;
    bool enable_key_sequenses_ = true;
    bool enable_remote_cursor_pos_ = false;

    QPixmap remote_cursor_shape_;
    QPoint remote_cursor_pos_;
    QPoint remote_cursor_hotspot_;

    QPoint prev_pos_;
    quint32 prev_mask_ = 0;

    std::set<quint32> remote_pressed_keys_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWidget);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_DESKTOP_WIDGET_H
