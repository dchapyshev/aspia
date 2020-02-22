//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__DESKTOP_WIDGET_H
#define CLIENT__UI__DESKTOP_WIDGET_H

#include "build/build_config.h"
#if defined(OS_WIN)
#include "base/win/scoped_user_object.h"
#endif // defined(OS_WIN)
#include "desktop/desktop_frame.h"

#include <QEvent>
#include <QWidget>

#include <memory>
#include <set>

namespace client {

class DesktopWidget : public QWidget
{
    Q_OBJECT

public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onPointerEvent(const QPoint& pos, uint32_t mask) = 0;
        virtual void onKeyEvent(uint32_t usb_keycode, uint32_t flags) = 0;
        virtual void onDrawDesktop() = 0;
    };

    DesktopWidget(Delegate* delegate, QWidget* parent);
    ~DesktopWidget() = default;

    desktop::Frame* desktopFrame();
    void setDesktopFrame(std::shared_ptr<desktop::Frame>& frame);

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

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void executeKeyEvent(uint32_t usb_keycode, uint32_t flags);

#if defined(OS_WIN)
    static LRESULT CALLBACK keyboardHookProc(INT code, WPARAM wparam, LPARAM lparam);

    base::win::ScopedHHOOK keyboard_hook_;
#endif // defined(OS_WIN)

    Delegate* delegate_;

    std::shared_ptr<desktop::Frame> frame_;
    bool enable_key_sequenses_ = true;

    QPoint prev_pos_;
    uint32_t prev_mask_ = 0;

    std::set<uint32_t> pressed_keys_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWidget);
};

} // namespace client

#endif // CLIENT__UI__DESKTOP_WIDGET_H
