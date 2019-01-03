//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__UI__DESKTOP_WIDGET_H_
#define ASPIA_CLIENT__UI__DESKTOP_WIDGET_H_

#include <QEvent>
#include <QWidget>

#include <memory>
#include <set>

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_user_object.h"
#endif

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class DesktopFrameQImage;

class DesktopWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopWidget(QWidget* parent);
    ~DesktopWidget() = default;

    void resizeDesktopFrame(const QSize& screen_size);
    DesktopFrame* desktopFrame();

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

signals:
    void sendKeyEvent(uint32_t usb_keycode, uint32_t flags);
    void sendPointerEvent(const QPoint& pos, uint32_t mask);

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

    ScopedHHOOK keyboard_hook_;
#endif // defined(OS_WIN)

    std::unique_ptr<DesktopFrameQImage> frame_;

    bool enable_key_sequenses_ = true;

    QPoint prev_pos_;
    uint32_t prev_mask_ = 0;

    std::set<uint32_t> pressed_keys_;

    DISALLOW_COPY_AND_ASSIGN(DesktopWidget);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__DESKTOP_WIDGET_H_
