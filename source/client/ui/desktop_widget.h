//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_WIDGET_H
#define CLIENT_UI_DESKTOP_WIDGET_H

#include "base/desktop/frame.h"
#include "build/build_config.h"
#include "proto/desktop.pb.h"

#if defined(OS_WIN)
#include "base/win/scoped_user_object.h"
#endif // defined(OS_WIN)

#include <QEvent>
#include <QPainter>
#include <QWidget>

#include <memory>
#include <set>

namespace client {

class DesktopWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopWidget(QWidget* parent);
    ~DesktopWidget() override;

    base::Frame* desktopFrame();
    void setDesktopFrame(std::shared_ptr<base::Frame>& frame);
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
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void executeKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void enableKeyHooks(bool enable);
    void releaseMouseButtons();
    void releaseKeyboardButtons();

    QPainter painter_;

#if defined(OS_WIN)
    static LRESULT CALLBACK keyboardHookProc(INT code, WPARAM wparam, LPARAM lparam);
    base::win::ScopedHHOOK keyboard_hook_;
#endif // defined(OS_WIN)

    std::shared_ptr<base::Frame> frame_;
    bool enable_key_sequenses_ = true;
    bool enable_remote_cursor_pos_ = false;

    QPixmap remote_cursor_shape_;
    QPoint remote_cursor_pos_;
    QPoint remote_cursor_hotspot_;

    QPoint prev_pos_;
    uint32_t prev_mask_ = 0;

    std::set<uint32_t> remote_pressed_keys_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWidget);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_WIDGET_H
