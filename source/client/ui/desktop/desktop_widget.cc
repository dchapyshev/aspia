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

#include "client/ui/desktop/desktop_widget.h"

#include "base/logging.h"
#include "base/desktop/frame_qimage.h"
#include "common/keycode_converter.h"

#include <QApplication>
#include <QWheelEvent>

#if defined(OS_LINUX)
#include <X11/XKBlib.h>
#if defined(KeyPress)
#undef KeyPress
#endif // defined(KeyPress)
#endif // defined(OS_LINUX)

#if defined(OS_MAC)
#include <Carbon/Carbon.h>
#include <CoreGraphics/CGEventSource.h>
#endif // defined(OS_MAC)

namespace client {

namespace {

constexpr uint32_t kWheelMask = proto::MouseEvent::WHEEL_DOWN | proto::MouseEvent::WHEEL_UP;

std::set<uint32_t> g_local_pressed_keys;

//--------------------------------------------------------------------------------------------------
bool isNumLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_NUMLOCK) != 0;
#elif defined(OS_LINUX)
    Display* display = XOpenDisplay(nullptr);
    if (!display)
    {
        LOG(LS_ERROR) << "XOpenDisplay failed";
        return false;
    }

    unsigned state = 0;
    XkbGetIndicatorState(display, XkbUseCoreKbd, &state);
    XCloseDisplay(display);

    return (state & 2) != 0;
#elif defined(OS_MAC)
    // Without NumLock.
    return true;
#else
#warning Platform support not implemented
    return false;
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
bool isCapsLockActivated()
{
#if defined(OS_WIN)
    return GetKeyState(VK_CAPITAL) != 0;
#elif defined(OS_LINUX)
    Display* display = XOpenDisplay(nullptr);
    if (!display)
    {
        LOG(LS_ERROR) << "XOpenDisplay failed";
        return false;
    }

    unsigned state = 0;
    XkbGetIndicatorState(display, XkbUseCoreKbd, &state);
    XCloseDisplay(display);

    return (state & 1) != 0;
#elif defined(OS_MAC)
    CGEventFlags event_flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
    return (event_flags & kCGEventFlagMaskAlphaShift) != 0;
#else
#warning Platform support not implemented
    return false;
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
bool isModifierKey(int key)
{
    return key == Qt::Key_Control || key == Qt::Key_Alt ||
           key == Qt::Key_Shift || key == Qt::Key_Meta;
}

const char* applicationStateToString(Qt::ApplicationState state)
{
    switch (state)
    {
        case Qt::ApplicationSuspended:
            return "ApplicationSuspended";
        case Qt::ApplicationHidden:
            return "ApplicationHidden";
        case Qt::ApplicationInactive:
            return "ApplicationInactive";
        case Qt::ApplicationActive:
            return "ApplicationActive";
        default:
            return "Unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopWidget::DesktopWidget(QWidget* parent)
    : QWidget(parent)
{
    LOG(LS_INFO) << "Ctor";

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);

    enableKeyHooks(true);

    connect(static_cast<QApplication*>(QApplication::instance()), &QApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState state)
    {
        LOG(LS_INFO) << "Application state changed: " << applicationStateToString(state);
        if (state != Qt::ApplicationActive)
        {
            releaseKeyboardButtons();
            releaseMouseButtons();
        }
    });
}

//--------------------------------------------------------------------------------------------------
DesktopWidget::~DesktopWidget()
{
    LOG(LS_INFO) << "Dtor";
    enableKeyHooks(false);
}

//--------------------------------------------------------------------------------------------------
base::Frame* DesktopWidget::desktopFrame()
{
    return frame_.get();
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::setDesktopFrame(std::shared_ptr<base::Frame> frame)
{
    frame_ = std::move(frame);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::setDesktopFrameError(proto::VideoErrorCode error_code)
{
    if (last_error_code_ == error_code)
        return;

    LOG(LS_INFO) << "Video error detected: " << error_code;
    last_error_code_ = error_code;

    if (error_timer_)
        delete error_timer_;

    error_timer_ = new QTimer(this);
    connect(error_timer_, &QTimer::timeout, this, [this]()
    {
        if (last_error_code_ != proto::VIDEO_ERROR_CODE_OK)
        {
            current_error_code_ = last_error_code_;

            if (frame_)
            {
                QImage* source_image = static_cast<base::FrameQImage*>(frame_.get())->image();
                error_image_ = std::make_unique<QImage>(
                    source_image->convertToFormat(QImage::Format_Grayscale8));
            }
        }

        error_timer_->deleteLater();
        update();
    });

    error_timer_->start(std::chrono::milliseconds(1500));
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::drawDesktopFrame()
{
    if (error_timer_)
        delete error_timer_;

    if (current_error_code_ != proto::VIDEO_ERROR_CODE_OK)
        error_image_.reset();

    last_error_code_ = proto::VIDEO_ERROR_CODE_OK;
    current_error_code_ = proto::VIDEO_ERROR_CODE_OK;

    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::setCursorShape(QPixmap&& cursor_shape, const QPoint& hotspot)
{
    remote_cursor_shape_ = std::move(cursor_shape);
    remote_cursor_hotspot_ = hotspot;

    if (remote_cursor_shape_.isNull())
    {
        setCursor(QCursor(Qt::ArrowCursor));
    }
    else
    {
        setCursor(QCursor(remote_cursor_shape_, hotspot.x(), hotspot.y()));
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::setCursorPosition(const QPoint& cursor_position)
{
    remote_cursor_pos_ = cursor_position;

    QSize widget_size = size();

    if (remote_cursor_pos_.x() < 0)
        remote_cursor_pos_.setX(0);
    else if (remote_cursor_pos_.x() > widget_size.width())
        remote_cursor_pos_.setX(widget_size.width());

    if (remote_cursor_pos_.y() < 0)
        remote_cursor_pos_.setY(0);
    else if (remote_cursor_pos_.y() > widget_size.height())
        remote_cursor_pos_.setY(widget_size.height());
}

//--------------------------------------------------------------------------------------------------
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

        if (buttons & Qt::BackButton)
            mask |= proto::MouseEvent::BACK_BUTTON;

        if (buttons & Qt::ForwardButton)
            mask |= proto::MouseEvent::FORWARD_BUTTON;
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
                emit sig_mouseEvent(event);
        }
        else
        {
            emit sig_mouseEvent(event);
        }
    }
}

//--------------------------------------------------------------------------------------------------
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

    uint32_t usb_keycode = common::KeycodeConverter::invalidUsbKeycode();

#if !defined(OS_MAC)
    usb_keycode = common::KeycodeConverter::nativeKeycodeToUsbKeycode(
        static_cast<int>(event->nativeScanCode()));
#else
    if (isModifierKey(key))
        usb_keycode = common::KeycodeConverter::qtKeycodeToUsbKeycode(key);
    else
        usb_keycode = common::KeycodeConverter::nativeKeycodeToUsbKeycode(event->nativeVirtualKey());
#endif

    if (usb_keycode == common::KeycodeConverter::invalidUsbKeycode())
        return;

    executeKeyEvent(usb_keycode, flags);
}

//--------------------------------------------------------------------------------------------------
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
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::ControlModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftCtrl);
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::ShiftModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftShift);
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::MetaModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftMeta);
        emit sig_keyEvent(event);
    }

    event.set_usb_keycode(key);
    emit sig_keyEvent(event);

    event.set_flags(flags);

    event.set_usb_keycode(key);
    emit sig_keyEvent(event);

    if (key_sequence & Qt::MetaModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftMeta);
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::ShiftModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftShift);
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::ControlModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftCtrl);
        emit sig_keyEvent(event);
    }

    if (key_sequence & Qt::AltModifier)
    {
        event.set_usb_keycode(kUsbCodeLeftAlt);
        emit sig_keyEvent(event);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::enableKeyCombinations(bool enable)
{
    enable_key_sequenses_ = enable;
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::enableRemoteCursorPosition(bool enable)
{
    enable_remote_cursor_pos_ = enable;
    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::userLeftFromWindow()
{
    releaseMouseButtons();
    releaseKeyboardButtons();
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::paintEvent(QPaintEvent* /* event */)
{
    painter_.begin(this);

#if !defined(OS_MAC)
    // SmoothPixmapTransform causes too much CPU load in MacOSX.
    painter_.setRenderHint(QPainter::SmoothPixmapTransform);
#endif
    if (current_error_code_ == proto::VIDEO_ERROR_CODE_OK)
    {
        base::FrameQImage* frame = reinterpret_cast<base::FrameQImage*>(frame_.get());
        if (frame)
        {
            painter_.drawImage(rect(), frame->constImage());

            if (enable_remote_cursor_pos_)
            {
                if (!remote_cursor_shape_.isNull())
                {
                    painter_.drawPixmap(QRect(remote_cursor_pos_ - remote_cursor_hotspot_,
                                              remote_cursor_shape_.size()),
                                        remote_cursor_shape_,
                                        remote_cursor_shape_.rect());
                }
                else
                {
                    painter_.setBrush(QBrush(Qt::black));
                    painter_.setPen(QPen(Qt::white));
                    painter_.drawEllipse(remote_cursor_pos_, 3, 3);
                }
            }
        }
    }
    else
    {
        const int kTableWidth = 400;
        const int kTableHeight = 100;
        const int kBorderSize = 1;
        const int kTitleHeight = 30;

        const QRect table_rect(width() / 2 - kTableWidth / 2,
                               height() / 2 - kTableHeight / 2,
                               kTableWidth,
                               kTableHeight);

        QRect title_rect(table_rect.x() + kBorderSize,
                         table_rect.y() + kBorderSize,
                         table_rect.width() - (kBorderSize * 2),
                         kTitleHeight);

        QRect message_rect(table_rect.x() + kBorderSize,
                           title_rect.bottom() + kBorderSize,
                           table_rect.width() - (kBorderSize * 2),
                           table_rect.height() - kTitleHeight - (kBorderSize * 2));

        if (error_image_)
            painter_.drawImage(rect(), *error_image_);

        painter_.fillRect(table_rect, QColor(167, 167, 167));
        painter_.fillRect(title_rect, QColor(207, 207, 207));
        painter_.fillRect(message_rect, QColor(255, 255, 255));

        QPixmap icon(QStringLiteral(":/img/main.png"));
        QPoint icon_pos(title_rect.x() + 8, title_rect.y() + (kTitleHeight / 2) - (icon.height() / 2));

        title_rect.setLeft(icon_pos.x() + icon.width() + 8);

        painter_.setPen(Qt::black);

        painter_.drawPixmap(icon_pos, icon);
        painter_.drawText(title_rect, Qt::AlignVCenter, QStringLiteral("Aspia"));

        QString message;
        switch (last_error_code_)
        {
            case proto::VIDEO_ERROR_CODE_PAUSED:
                message = tr("The session was paused by a remote user");
                break;

            case proto::VIDEO_ERROR_CODE_TEMPORARY:
                message = tr("The session is temporarily unavailable");
                break;

            case proto::VIDEO_ERROR_CODE_PERMANENT:
                message = tr("The session is permanently unavailable");
                break;

            default:
                message = tr("Error while receiving video stream: %1").arg(last_error_code_);
                break;
        }

        painter_.drawText(message_rect, Qt::AlignCenter, message);
    }

    painter_.end();
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::mouseMoveEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::mousePressEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::mouseReleaseEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    doMouseEvent(event->type(), event->buttons(), event->pos());
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::keyPressEvent(QKeyEvent* event)
{
    doKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::keyReleaseEvent(QKeyEvent* event)
{
    doKeyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::leaveEvent(QEvent* event)
{
    // When the mouse cursor leaves the widget area, release all the mouse buttons.
    releaseMouseButtons();
    QWidget::leaveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::focusOutEvent(QFocusEvent* event)
{
    userLeftFromWindow();
    QWidget::focusOutEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::executeKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    if (flags & proto::KeyEvent::PRESSED)
        remote_pressed_keys_.insert(usb_keycode);
    else
        remote_pressed_keys_.erase(usb_keycode);

    proto::KeyEvent event;
    event.set_usb_keycode(usb_keycode);
    event.set_flags(flags);

    emit sig_keyEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::enableKeyHooks(bool enable)
{
#if defined(OS_WIN)
    if (enable)
        keyboard_hook_.reset(SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, nullptr, 0));
    else
        keyboard_hook_.reset();
#elif defined(OS_MAC)
    if (enable)
    {
        const CGEventMask keyboard_mask =
            CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

        event_tap_ = CGEventTapCreate(kCGHIDEventTap,
                                      kCGHeadInsertEventTap,
                                      kCGEventTapOptionDefault,
                                      keyboard_mask,
                                      keyboardFilterProc,
                                      this);
        if (!event_tap_)
        {
            LOG(LS_ERROR) << "CGEventTapCreate failed";
            return;
        }

        CFRunLoopSourceRef run_loop_source =
            CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap_.get(), 0);

        CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
        CGEventTapEnable(event_tap_.get(), true);
    }
    else
    {
        if (event_tap_)
            CGEventTapEnable(event_tap_.get(), false);
    }
#else
    Q_UNUSED(enable)
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::releaseMouseButtons()
{
    if (prev_mask_ == 0)
        return;

    proto::MouseEvent mouse_event;
    mouse_event.set_x(prev_pos_.x());
    mouse_event.set_y(prev_pos_.y());

    emit sig_mouseEvent(mouse_event);
    prev_mask_ = 0;
}

//--------------------------------------------------------------------------------------------------
void DesktopWidget::releaseKeyboardButtons()
{
    if (remote_pressed_keys_.empty())
        return;

    uint32_t flags = 0;
    flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

    proto::KeyEvent key_event;
    key_event.set_flags(flags);

    auto it = remote_pressed_keys_.begin();
    while (it != remote_pressed_keys_.end())
    {
        key_event.set_usb_keycode(*it);
        emit sig_keyEvent(key_event);
        it = remote_pressed_keys_.erase(it);
    }
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
// static
LRESULT CALLBACK DesktopWidget::keyboardHookProc(INT code, WPARAM wparam, LPARAM lparam)
{
    if (code == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* hook = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
        if (hook->vkCode != VK_CAPITAL && hook->vkCode != VK_NUMLOCK)
        {
            uint32_t flags = ((wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) ?
                proto::KeyEvent::PRESSED : 0);
            uint32_t scan_code = hook->scanCode;

            if (hook->flags & LLKHF_EXTENDED)
                scan_code |= 0x100;

            uint32_t usb_keycode = common::KeycodeConverter::nativeKeycodeToUsbKeycode(
                static_cast<int>(scan_code));
            if (usb_keycode != common::KeycodeConverter::invalidUsbKeycode())
            {
                DesktopWidget* self = dynamic_cast<DesktopWidget*>(QApplication::focusWidget());
                WId foreground_window = reinterpret_cast<WId>(GetForegroundWindow());
                bool is_foreground = false;

                if (self && self->parentWidget()->window()->winId() == foreground_window)
                    is_foreground = true;

                if (is_foreground && self->enable_key_sequenses_)
                {
                    flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
                    flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

                    self->executeKeyEvent(usb_keycode, flags);

                    if (flags & proto::KeyEvent::PRESSED)
                        return TRUE;

                    auto result = g_local_pressed_keys.find(usb_keycode);
                    if (result == g_local_pressed_keys.end())
                        return TRUE;

                    g_local_pressed_keys.erase(usb_keycode);
                }
                else
                {
                    if (flags & proto::KeyEvent::PRESSED)
                        g_local_pressed_keys.insert(usb_keycode);
                    else
                        g_local_pressed_keys.erase(usb_keycode);
                }
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}
#endif // defined(OS_WIN)

#if defined(OS_MAC)
//--------------------------------------------------------------------------------------------------
// static
CGEventRef DesktopWidget::keyboardFilterProc(
    CGEventTapProxy /* proxy */, CGEventType type, CGEventRef event, void* user_info)
{
    DesktopWidget* self_widget = reinterpret_cast<DesktopWidget*>(user_info);
    DesktopWidget* focus_widget = dynamic_cast<DesktopWidget*>(QApplication::focusWidget());

    if (self_widget && focus_widget && self_widget == focus_widget &&
        self_widget->enable_key_sequenses_)
    {
        uint32_t flags = ((type == kCGEventKeyDown) ? proto::KeyEvent::PRESSED : 0);
        flags |= (isCapsLockActivated() ? proto::KeyEvent::CAPSLOCK : 0);
        flags |= (isNumLockActivated() ? proto::KeyEvent::NUMLOCK : 0);

        CGKeyCode key_code = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

        uint32_t usb_keycode = common::KeycodeConverter::nativeKeycodeToUsbKeycode(
            static_cast<int>(key_code));
        if (usb_keycode != common::KeycodeConverter::invalidUsbKeycode())
        {
            self_widget->executeKeyEvent(usb_keycode, flags);
            return nullptr;
        }
        else
        {
            LOG(LS_ERROR) << "Unable to convert key code: " << key_code;
        }
    }

    return event;
}
#endif // defined(OS_MAC)

} // namespace client
