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
#include "proto/desktop.pb.h"

#include <QApplication>
#include <QPainter>
#include <QWheelEvent>

namespace client {

namespace {

constexpr uint32_t kWheelMask = proto::PointerEvent::WHEEL_DOWN | proto::PointerEvent::WHEEL_UP;

bool isNumLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_NUMLOCK) != 0;
#else
#error Platform support not implemented
#endif // defined(OS_WIN)
}

bool isCapsLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_CAPITAL) != 0;
#else
#error Platform support not implemented
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

desktop::Frame* DesktopWidget::desktopFrame()
{
    return frame_.get();
}

void DesktopWidget::setDesktopFrame(std::shared_ptr<desktop::Frame>& frame)
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
            mask |= proto::PointerEvent::LEFT_BUTTON;

        if (buttons & Qt::MiddleButton)
            mask |= proto::PointerEvent::MIDDLE_BUTTON;

        if (buttons & Qt::RightButton)
            mask |= proto::PointerEvent::RIGHT_BUTTON;
    }

    int wheel_steps = 0;

    if (event_type == QEvent::Wheel)
    {
        if (delta.y() < 0)
        {
            mask |= proto::PointerEvent::WHEEL_DOWN;
            wheel_steps = -delta.y() / QWheelEvent::DefaultDeltasPerStep;
        }
        else
        {
            mask |= proto::PointerEvent::WHEEL_UP;
            wheel_steps = delta.y() / QWheelEvent::DefaultDeltasPerStep;
        }

        if (!wheel_steps)
            wheel_steps = 1;
    }

    if (prev_pos_ != pos || prev_mask_ != mask)
    {
        prev_pos_ = pos;
        prev_mask_ = mask & ~kWheelMask;

        if (mask & kWheelMask)
        {
            for (int i = 0; i < wheel_steps; ++i)
            {
                delegate_->onPointerEvent(pos, mask);
                delegate_->onPointerEvent(pos, mask & ~kWheelMask);
            }
        }
        else
        {
            delegate_->onPointerEvent(pos, mask);
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

    QVector<int> keys;

    if (key_sequence & Qt::AltModifier)
        keys.push_back(kUsbCodeLeftAlt);

    if (key_sequence & Qt::ControlModifier)
        keys.push_back(kUsbCodeLeftCtrl);

    if (key_sequence & Qt::ShiftModifier)
        keys.push_back(kUsbCodeLeftShift);

    if (key_sequence & Qt::MetaModifier)
        keys.push_back(kUsbCodeLeftMeta);

    uint32_t key = common::KeycodeConverter::qtKeycodeToUsbKeycode(
        key_sequence & ~Qt::KeyboardModifierMask);
    if (key == common::KeycodeConverter::invalidUsbKeycode())
        return;

    keys.push_back(key);

    uint32_t flags = proto::KeyEvent::PRESSED;

    flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

    for (auto it = keys.cbegin(); it != keys.cend(); ++it)
        executeKeyEvent(*it, flags);

    flags ^= proto::KeyEvent::PRESSED;

    for (auto it = keys.crbegin(); it != keys.crend(); ++it)
        executeKeyEvent(*it, flags);
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
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(rect(), frame->constImage());
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

void DesktopWidget::wheelEvent(QWheelEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos(), event->angleDelta());
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
    // When the mouse cursor leaves the widget area, release all the mouse buttons.
    if (prev_mask_ != 0)
    {
        delegate_->onPointerEvent(prev_pos_, 0);
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

        for (const auto& key : pressed_keys_)
            executeKeyEvent(key, flags);
    }

    QWidget::focusOutEvent(event);
}

void DesktopWidget::executeKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    if (flags & proto::KeyEvent::PRESSED)
        pressed_keys_.insert(usb_keycode);
    else
        pressed_keys_.erase(usb_keycode);

    delegate_->onKeyEvent(usb_keycode, flags);
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
