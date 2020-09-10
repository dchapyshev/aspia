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

#include "client/ui/desktop_widget.h"

#include "common/keycode_converter.h"
#include "client/ui/frame_qimage.h"

#include <QApplication>
#include <QWheelEvent>

namespace client {

namespace {

constexpr uint32_t kWheelMask = proto::MouseEvent::WHEEL_DOWN | proto::MouseEvent::WHEEL_UP;

bool isNumLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_NUMLOCK) != 0;
#else
#warning Platform support not implemented
    return false;
#endif // defined(OS_WIN)
}

bool isCapsLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_CAPITAL) != 0;
#else
#warning Platform support not implemented
    return false;
#endif // defined(OS_WIN)
}

bool isModifierKey(int key)
{
    return key == Qt::Key_Control || key == Qt::Key_Alt ||
           key == Qt::Key_Shift || key == Qt::Key_Meta;
}

} // namespace

DesktopWidget::DesktopWidget(Delegate* delegate, QWidget* parent)
    : QWidget(parent),
      delegate_(delegate)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
}

base::Frame* DesktopWidget::desktopFrame()
{
    return frame_.get();
}

void DesktopWidget::setDesktopFrame(std::shared_ptr<base::Frame>& frame)
{
    frame_ = std::move(frame);
}

void DesktopWidget::doMouseEvent(QEvent::Type event_type,
                                 const Qt::MouseButtons& buttons,
                                 const QPoint& pos,
                                 const QPoint& delta)
{
    if (!frame_)
        return;

    uint32_t mask;

    if (event_type == QMouseEvent::MouseMove)
    {
        mask = prev_mask_;
    }
    else
    {
        mask = 0;

        if (buttons & Qt::LeftButton)
            mask |= proto::MouseEvent::LEFT_BUTTON;

        if (buttons & Qt::MiddleButton)
            mask |= proto::MouseEvent::MIDDLE_BUTTON;

        if (buttons & Qt::RightButton)
            mask |= proto::MouseEvent::RIGHT_BUTTON;
    }

    int wheel_steps = 0;

    if (event_type == QEvent::Wheel)
    {
        if (delta.y() < 0)
        {
            mask |= proto::MouseEvent::WHEEL_DOWN;
            wheel_steps = -delta.y() / QWheelEvent::DefaultDeltasPerStep;
        }
        else
        {
            mask |= proto::MouseEvent::WHEEL_UP;
            wheel_steps = delta.y() / QWheelEvent::DefaultDeltasPerStep;
        }

        if (!wheel_steps)
            wheel_steps = 1;
    }

    if (prev_pos_ != pos || prev_mask_ != mask)
    {
        prev_pos_ = pos;
        prev_mask_ = mask & ~kWheelMask;

        proto::MouseEvent event;
        event.set_x(pos.x());
        event.set_y(pos.y());
        event.set_mask(mask);

        if (mask & kWheelMask)
        {
            for (int i = 0; i < wheel_steps; ++i)
                delegate_->onMouseEvent(event);
        }
        else
        {
            delegate_->onMouseEvent(event);
        }
    }
}

void DesktopWidget::doKeyEvent(QKeyEvent* event)
{
    int key = event->key();
    if (key == Qt::Key_CapsLock || key == Qt::Key_NumLock)
        return;

    if (!enable_key_sequenses_ && isModifierKey(key))
        return;

    uint32_t flags = ((event->type() == QEvent::KeyPress) ? proto::KeyEvent::PRESSED : 0);

    flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

    uint32_t usb_keycode =
        common::KeycodeConverter::nativeKeycodeToUsbKeycode(event->nativeScanCode());
    if (usb_keycode == common::KeycodeConverter::invalidUsbKeycode())
        return;

    executeKeyEvent(usb_keycode, flags);
}

void DesktopWidget::executeKeyCombination(int key_sequence)
{
    const uint32_t kUsbCodeLeftAlt = 0x0700e2;
    const uint32_t kUsbCodeLeftCtrl = 0x0700e0;
    const uint32_t kUsbCodeLeftShift = 0x0700e1;
    const uint32_t kUsbCodeLeftMeta = 0x0700e3;

    uint32_t flags = 0;

    flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

    uint32_t key = common::KeycodeConverter::qtKeycodeToUsbKeycode(
        key_sequence & ~Qt::KeyboardModifierMask);
    if (key == common::KeycodeConverter::invalidUsbKeycode())
        return;

    proto::KeyEvent event;
    event.set_flags(flags | proto::KeyEvent::PRESSED);

    if (key_sequence & Qt::AltModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftAlt);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::ControlModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftCtrl);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::ShiftModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftShift);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::MetaModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftMeta);
        delegate_->onKeyEvent(event);
    }

    event.set_usb_keycode(key);
    delegate_->onKeyEvent(event);

    event.set_flags(flags);

    event.set_usb_keycode(key);
    delegate_->onKeyEvent(event);

    if (key_sequence & Qt::MetaModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftMeta);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::ShiftModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftShift);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::ControlModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftCtrl);
        delegate_->onKeyEvent(event);
    }

    if (key_sequence & Qt::AltModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftAlt);
        delegate_->onKeyEvent(event);
    }
}

void DesktopWidget::enableKeyCombinations(bool enable)
{
    enable_key_sequenses_ = enable;

#if defined(OS_WIN)
    if (enable)
        keyboard_hook_.reset(SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, nullptr, 0));
    else
        keyboard_hook_.reset();
#endif // defined(OS_WIN)
}

void DesktopWidget::paintEvent(QPaintEvent* /* event */)
{
    FrameQImage* frame = reinterpret_cast<FrameQImage*>(frame_.get());
    if (frame)
    {
        painter_.begin(this);
        painter_.setRenderHint(QPainter::SmoothPixmapTransform);
        painter_.drawImage(rect(), frame->constImage());
        painter_.end();
    }

    delegate_->onDrawDesktop();
}

void DesktopWidget::mouseMoveEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mousePressEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mouseReleaseEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::keyPressEvent(QKeyEvent* event)
{
    doKeyEvent(event);
}

void DesktopWidget::keyReleaseEvent(QKeyEvent* event)
{
    doKeyEvent(event);
}

void DesktopWidget::leaveEvent(QEvent* event)
{
#if defined(OS_WIN)
    keyboard_hook_.reset();
#endif // defined(OS_WIN)

    // When the mouse cursor leaves the widget area, release all the mouse buttons.
    if (prev_mask_ != 0)
    {
        proto::MouseEvent mouse_event;
        mouse_event.set_x(prev_pos_.x());
        mouse_event.set_y(prev_pos_.y());

        delegate_->onMouseEvent(mouse_event);
        prev_mask_ = 0;
    }

    QWidget::leaveEvent(event);
}

void DesktopWidget::focusInEvent(QFocusEvent* event)
{
#if defined(OS_WIN)
    if (enable_key_sequenses_)
        keyboard_hook_.reset(SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, nullptr, 0));
#endif // defined(OS_WIN)

    QWidget::focusInEvent(event);
}

void DesktopWidget::focusOutEvent(QFocusEvent* event)
{
#if defined(OS_WIN)
    keyboard_hook_.reset();
#endif // defined(OS_WIN)

    // Release all pressed keys of the keyboard.
    if (!pressed_keys_.empty())
    {
        uint32_t flags = 0;

        flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
        flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

        proto::KeyEvent key_event;
        key_event.set_flags(flags);

        auto it = pressed_keys_.begin();
        while (it != pressed_keys_.end())
        {
            key_event.set_usb_keycode(*it);
            delegate_->onKeyEvent(key_event);
            it = pressed_keys_.erase(it);
        }
    }

    QWidget::focusOutEvent(event);
}

void DesktopWidget::executeKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    if (flags & proto::KeyEvent::PRESSED)
        pressed_keys_.insert(usb_keycode);
    else
        pressed_keys_.erase(usb_keycode);

    proto::KeyEvent event;
    event.set_usb_keycode(usb_keycode);
    event.set_flags(flags);

    delegate_->onKeyEvent(event);
}

#if defined(OS_WIN)
// static
LRESULT CALLBACK DesktopWidget::keyboardHookProc(INT code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION)
    {
        DesktopWidget* self = dynamic_cast<DesktopWidget*>(QApplication::focusWidget());
        if (self)
        {
            KBDLLHOOKSTRUCT* hook = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);

            if (hook->vkCode != VK_CAPITAL && hook->vkCode != VK_NUMLOCK)
            {
                uint32_t flags = ((wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) ?
                                  proto::KeyEvent::PRESSED : 0);

                flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
                flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

                uint32_t scan_code = hook->scanCode;

                if (hook->flags & LLKHF_EXTENDED)
                    scan_code |= 0x100;

                uint32_t usb_keycode =
                    common::KeycodeConverter::nativeKeycodeToUsbKeycode(scan_code);
                if (usb_keycode != common::KeycodeConverter::invalidUsbKeycode())
                {
                    self->executeKeyEvent(usb_keycode, flags);
                    return TRUE;
                }
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}
#endif // defined(OS_WIN)

} // namespace client
