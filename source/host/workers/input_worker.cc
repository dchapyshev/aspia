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

#include "host/workers/input_worker.h"

#include <climits>

#include "base/logging.h"
#include "base/threading/worker_manager.h"
#include "host/input_injector.h"
#include "host/workers/ipc_worker.h"
#include "host/workers/screen_worker.h"
#include "proto/desktop_input.h"

#if defined(Q_OS_WINDOWS)
#include "host/input_injector_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
#include "host/input_injector_mac.h"
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_LINUX)
#include "host/input_injector_uinput.h"
#include "host/input_injector_x11.h"
#endif // defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
// On Windows the injection thread must be able to follow the active input desktop (winlogon/UAC)
// with SetThreadDesktop(), which fails for a thread that owns windows - and the Qt dispatcher pumps
// events through an invisible message window. The asio dispatcher has no such window. On macOS the
// Qt dispatcher is used for consistency with the other non-I/O workers (CFRunLoop-backed).
InputWorker::InputWorker()
#if defined(Q_OS_MACOS)
    : Worker(Thread::QtDispatcher)
#else
    : Worker(Thread::AsioDispatcher)
#endif
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
InputWorker::~InputWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onInjectKeyEvent(const proto::input::KeyEvent& event)
{
    if (is_paused_ || is_keyboard_locked_)
        return;

    if (input_injector_)
    {
        input_injector_->injectKeyEvent(event);
    }
    else if (screen_worker_)
    {
        QPointer<ScreenWorker> worker = screen_worker_;
        QMetaObject::invokeMethod(worker, [worker, event]()
        {
            if (worker)
                worker->injectKeyEvent(event);
        }, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onInjectTextEvent(const proto::input::TextEvent& event)
{
    if (is_paused_ || is_keyboard_locked_)
        return;

    if (input_injector_)
    {
        input_injector_->injectTextEvent(event);
    }
    else if (screen_worker_)
    {
        QPointer<ScreenWorker> worker = screen_worker_;
        QMetaObject::invokeMethod(worker, [worker, event]()
        {
            if (worker)
                worker->injectTextEvent(event);
        }, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onInjectMouseEvent(const proto::input::MouseEvent& event)
{
    if (is_paused_ || is_mouse_locked_)
        return;

    if (scale_x_ <= 0.0 || scale_y_ <= 0.0)
        return;

    // The client coordinates are untrusted: multiply in double (the int multiply overflows for values
    // beyond ~21.4M) and clamp back into int range before the narrowing cast, which is undefined for
    // out-of-range values. The injectors clamp the result into the actual screen.
    int pos_x = int(qBound<double>(INT_MIN, double(event.x()) * 100 / scale_x_, INT_MAX));
    int pos_y = int(qBound<double>(INT_MIN, double(event.y()) * 100 / scale_y_, INT_MAX));

    proto::input::MouseEvent out_event;
    out_event.set_mask(event.mask());
    out_event.set_x(pos_x);
    out_event.set_y(pos_y);

    if (input_injector_)
    {
        input_injector_->injectMouseEvent(out_event);
    }
    else if (screen_worker_)
    {
        QPointer<ScreenWorker> worker = screen_worker_;
        QMetaObject::invokeMethod(worker, [worker, out_event]()
        {
            if (worker)
                worker->injectMouseEvent(out_event);
        }, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onInjectTouchEvent(const proto::input::TouchEvent& event)
{
    if (is_paused_ || is_mouse_locked_ || is_keyboard_locked_)
        return;

    if (input_injector_)
    {
        input_injector_->injectTouchEvent(event);
    }
    else if (screen_worker_)
    {
        QPointer<ScreenWorker> worker = screen_worker_;
        QMetaObject::invokeMethod(worker, [worker, event]()
        {
            if (worker)
                worker->injectTouchEvent(event);
        }, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onSetPaused(bool paused)
{
    is_paused_ = paused;
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onSetMouseLocked(bool locked)
{
    is_mouse_locked_ = locked;
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onSetKeyboardLocked(bool locked)
{
    is_keyboard_locked_ = locked;
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onSetBlockInput(bool enable)
{
    if (input_injector_)
    {
        input_injector_->setBlockInput(enable);
    }
    else if (screen_worker_)
    {
        QPointer<ScreenWorker> worker = screen_worker_;
        QMetaObject::invokeMethod(worker, [worker, enable]()
        {
            if (worker)
                worker->setBlockInput(enable);
        }, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onStart()
{
    LOG(INFO) << "Input worker started";

    screen_worker_ = WorkerManager::instance().find<ScreenWorker>();
    if (screen_worker_)
    {
        connect(screen_worker_, &ScreenWorker::sig_scaleFactorChanged,
                this, &InputWorker::onScaleFactorChanged, Qt::QueuedConnection);
        connect(screen_worker_, &ScreenWorker::sig_screenInfoChanged,
                this, &InputWorker::onScreenInfoChanged, Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "Screen worker not found";
    }

    // Subscribe to the client input and gating commands forwarded by the IPC worker.
    ipc_worker_ = WorkerManager::instance().find<IpcWorker>();
    if (ipc_worker_)
    {
        connect(ipc_worker_, &IpcWorker::sig_injectKeyEvent, this, &InputWorker::onInjectKeyEvent,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_injectTextEvent, this, &InputWorker::onInjectTextEvent,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_injectMouseEvent, this, &InputWorker::onInjectMouseEvent,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_injectTouchEvent, this, &InputWorker::onInjectTouchEvent,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_paused, this, &InputWorker::onSetPaused,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_mouseLocked, this, &InputWorker::onSetMouseLocked,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_keyboardLocked, this, &InputWorker::onSetKeyboardLocked,
                Qt::QueuedConnection);
        connect(ipc_worker_, &IpcWorker::sig_blockInput, this, &InputWorker::onSetBlockInput,
                Qt::QueuedConnection);
    }
    else
    {
        LOG(ERROR) << "IPC worker not found";
    }
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onStop()
{
    LOG(INFO) << "Input worker stopped";

    if (screen_worker_)
    {
        screen_worker_->disconnect(this);
        screen_worker_ = nullptr;
    }

    if (ipc_worker_)
    {
        ipc_worker_->disconnect(this);
        ipc_worker_ = nullptr;
    }

    input_injector_.reset();
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onScaleFactorChanged(double scale_x, double scale_y)
{
    scale_x_ = scale_x;
    scale_y_ = scale_y;
}

//--------------------------------------------------------------------------------------------------
void InputWorker::onScreenInfoChanged(
    ScreenCapturer::Type type, const QSize& screen_size, const QPoint& offset)
{
    screen_size_ = screen_size;
    screen_offset_ = offset;

    updateInjectorForCaptureType(type);

    if (input_injector_)
        input_injector_->setScreenInfo(screen_size, offset);
}

//--------------------------------------------------------------------------------------------------
void InputWorker::updateInjectorForCaptureType(ScreenCapturer::Type type)
{
    InputInjector::Type desired = InputInjector::Type::UNKNOWN;
    switch (type)
    {
        case ScreenCapturer::Type::WIN_GDI:
        case ScreenCapturer::Type::WIN_DXGI:
            desired = InputInjector::Type::WINDOWS;
            break;

        case ScreenCapturer::Type::MACOSX:
            desired = InputInjector::Type::MAC;
            break;

        case ScreenCapturer::Type::LINUX_X11:
            desired = InputInjector::Type::X11;
            break;

        case ScreenCapturer::Type::LINUX_KMS:
        case ScreenCapturer::Type::LINUX_KWIN:
        case ScreenCapturer::Type::LINUX_WLR:
            desired = InputInjector::Type::UINPUT;
            break;

        case ScreenCapturer::Type::LINUX_VT:
        case ScreenCapturer::Type::LINUX_WAYLAND:
            // Injection delegated to ScreenWorker; desired stays UNKNOWN.
            break;

        case ScreenCapturer::Type::UNKNOWN:
            // The capturer is being reselected; drop the injector until the new type arrives.
            // Desired stays UNKNOWN.
            break;

        default:
            LOG(ERROR) << "Unexpected capture type:" << type;
            return;
    }

    if (desired == InputInjector::Type::UNKNOWN)
    {
        // No injector owned here: either injection is delegated to ScreenWorker (VT/Wayland) or the
        // capturer is being reselected. Drop any injector this worker held.
        input_injector_.reset();
        return;
    }

    // Already have the right injector (WIN_GDI/WIN_DXGI map to the same one, so no recreation on a
    // GDI<->DXGI switch).
    if (input_injector_ && input_injector_->type() == desired)
        return;

    // Release the previous injector before creating the replacement.
    input_injector_.reset();

    switch (desired)
    {
#if defined(Q_OS_WINDOWS)
        case InputInjector::Type::WINDOWS:
            input_injector_ = new InputInjectorWin(this);
            break;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
        case InputInjector::Type::MAC:
            input_injector_ = InputInjectorMac::create(this);
            if (!input_injector_)
                LOG(ERROR) << "Unable to create input injector";
            break;
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_LINUX)
        case InputInjector::Type::X11:
            input_injector_ = InputInjectorX11::create();
            if (!input_injector_)
                LOG(ERROR) << "Unable to create X11 input injector";
            break;

        case InputInjector::Type::UINPUT:
            input_injector_ = InputInjectorUinput::create(this);
            if (!input_injector_)
                LOG(ERROR) << "Unable to create uinput input injector";
            break;
#endif // defined(Q_OS_LINUX)

        default:
            // Injector types not owned by this worker (VT/Wayland) never reach here.
            break;
    }
}
