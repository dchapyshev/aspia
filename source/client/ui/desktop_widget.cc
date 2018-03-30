//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_widget.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/desktop_widget.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(Q_OS_WIN)

#include "proto/desktop_session.pb.h"
#include "protocol/keycode_converter.h"
#include "desktop_capture/desktop_frame_qimage.h"

namespace aspia {

namespace {

constexpr quint32 kWheelMask =
    proto::desktop::PointerEvent::WHEEL_DOWN | proto::desktop::PointerEvent::WHEEL_UP;

bool isNumLockActivated()
{
#if defined(Q_OS_WIN)
    return GetKeyState(VK_NUMLOCK) != 0;
#else
#error Platform support not implemented
#endif // defined(Q_OS_WIN)
}

bool isCapsLockActivated()
{
#if defined(Q_OS_WIN)
    return GetKeyState(VK_CAPITAL) != 0;
#else
#error Platform support not implemented
#endif // defined(Q_OS_WIN)
}

} // namespace

DesktopWidget::DesktopWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
}

void DesktopWidget::resizeDesktopFrame(const QSize& screen_size)
{
    frame_ = DesktopFrameQImage::create(screen_size);
    resize(screen_size);
}

void DesktopWidget::drawDesktopFrame()
{
    update();
}

DesktopFrame* DesktopWidget::desktopFrame()
{
    return frame_.get();
}

void DesktopWidget::executeKeySequense(int key_sequence)
{
    const quint32 kUsbCodeLeftAlt = 0x0700e2;
    const quint32 kUsbCodeLeftCtrl = 0x0700e0;
    const quint32 kUsbCodeLeftShift = 0x0700e1;
    const quint32 kUsbCodeLeftMeta = 0x0700e3;

    QVector<int> keys;

    if (key_sequence & Qt::AltModifier)
        keys.push_back(kUsbCodeLeftAlt);

    if (key_sequence & Qt::ControlModifier)
        keys.push_back(kUsbCodeLeftCtrl);

    if (key_sequence & Qt::ShiftModifier)
        keys.push_back(kUsbCodeLeftShift);

    if (key_sequence & Qt::MetaModifier)
        keys.push_back(kUsbCodeLeftMeta);

    int key = KeycodeConverter::QtKeycodeToUsbKeycode(key_sequence & ~Qt::KeyboardModifierMask);
    if (key == KeycodeConverter::InvalidUsbKeycode())
        return;

    keys.push_back(key);

    quint32 flags = proto::desktop::KeyEvent::PRESSED;

    flags |= (isCapsLockActivated() ? proto::desktop::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::desktop::KeyEvent::NUMLOCK : 0);

    for (auto it = keys.begin(); it != keys.end(); ++it)
        emit sendKeyEvent(*it, flags);

    flags ^= proto::desktop::KeyEvent::PRESSED;

    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        emit sendKeyEvent(*it, flags);
}

void DesktopWidget::paintEvent(QPaintEvent* event)
{
    if (frame_)
    {
        QPainter painter(this);
        painter.drawImage(rect(), frame_->image());
    }

    emit updated();
}

void DesktopWidget::mouseMoveEvent(QMouseEvent* event)
{
    processMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mousePressEvent(QMouseEvent* event)
{
    processMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mouseReleaseEvent(QMouseEvent* event)
{
    processMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    processMouseEvent(event->type(), event->buttons(), event->pos());
}

void DesktopWidget::wheelEvent(QWheelEvent* event)
{
    processMouseEvent(event->type(), event->buttons(), event->pos(), event->angleDelta());
}

void DesktopWidget::keyPressEvent(QKeyEvent* event)
{
    processKeyEvent(event);
}

void DesktopWidget::keyReleaseEvent(QKeyEvent* event)
{
    processKeyEvent(event);
}

void DesktopWidget::leaveEvent(QEvent *event)
{
    // When the mouse cursor leaves the widget area, release all the mouse buttons.
    if (prev_mask_ != 0)
    {
        emit sendPointerEvent(prev_pos_, 0);
        prev_mask_ = 0;
    }

    QWidget::leaveEvent(event);
}

void DesktopWidget::processMouseEvent(QEvent::Type event_type,
                                      const Qt::MouseButtons& buttons,
                                      const QPoint& pos,
                                      const QPoint& delta)
{
    if (!frame_ || !frame_->contains(pos.x(), pos.y()))
        return;

    quint32 mask;

    if (event_type == QMouseEvent::MouseMove)
    {
        mask = prev_mask_;
    }
    else
    {
        mask = 0;

        if (buttons & Qt::LeftButton)
            mask |= proto::desktop::PointerEvent::LEFT_BUTTON;

        if (buttons & Qt::MiddleButton)
            mask |= proto::desktop::PointerEvent::MIDDLE_BUTTON;

        if (buttons & Qt::RightButton)
            mask |= proto::desktop::PointerEvent::RIGHT_BUTTON;
    }

    int wheel_steps = 0;

    if (event_type == QEvent::Wheel)
    {
        if (delta.y() < 0)
        {
            mask |= proto::desktop::PointerEvent::WHEEL_DOWN;
            wheel_steps = -delta.y() / 120;
        }
        else
        {
            mask |= proto::desktop::PointerEvent::WHEEL_UP;
            wheel_steps = delta.y() / 120;
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
                emit sendPointerEvent(pos, mask);
                emit sendPointerEvent(pos, mask & ~kWheelMask);
            }
        }
        else
        {
            emit sendPointerEvent(pos, mask);
        }
    }
}

void DesktopWidget::processKeyEvent(QKeyEvent* event)
{
    int key = event->key();
    if (key == Qt::Key_CapsLock || key == Qt::Key_NumLock)
        return;

    uint32_t flags = ((event->type() == QEvent::KeyPress) ? proto::desktop::KeyEvent::PRESSED : 0);

    flags |= (isCapsLockActivated() ? proto::desktop::KeyEvent::CAPSLOCK : 0);
    flags |= (isNumLockActivated() ? proto::desktop::KeyEvent::NUMLOCK : 0);

    quint32 usb_keycode = KeycodeConverter::NativeKeycodeToUsbKeycode(event->nativeScanCode());
    if (usb_keycode == KeycodeConverter::InvalidUsbKeycode())
        return;

    emit sendKeyEvent(usb_keycode, flags);
}

} // namespace aspia
