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

#include "base/power_save_blocker.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_thread_desktop.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <linux/uinput.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>

#include <QDBusInterface>
#include <QDBusReply>
#include <QString>

#include "base/linux/session_dbus.h"
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

namespace {

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
const char kScreenSaverService[] = "org.freedesktop.ScreenSaver";
const char kScreenSaverPath[] = "/org/freedesktop/ScreenSaver";
const char kScreenSaverIface[] = "org.freedesktop.ScreenSaver";

// Time to let the display server / compositor enumerate the new uinput device (via udev) before
// feeding it events - otherwise the motion is emitted before anyone is reading the device and lost.
const int kUinputSettleMs = 400;

//--------------------------------------------------------------------------------------------------
// The screen-saver inhibit prevents the display from blanking but cannot turn a display that is
// already off back on. The only display-server-agnostic way (X11 and Wayland alike) to wake it is to
// feed a real input event through the kernel input subsystem: DPMS / idle wake keys off kernel input
// events regardless of who produced them. We create a throwaway virtual pointer via uinput, nudge it
// (net-zero, so the cursor stays put), and remove it. Requires write access to /dev/uinput, i.e. the
// process must run as root.
void wakeUpDisplay()
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
    {
        PLOG(ERROR) << "Unable to open /dev/uinput";
        return;
    }

    // A relative pointer with a button. Some input stacks ignore a pointer that exposes no buttons.
    if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0 ||
        ioctl(fd, UI_SET_RELBIT, REL_X) < 0 ||
        ioctl(fd, UI_SET_RELBIT, REL_Y) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0)
    {
        PLOG(ERROR) << "uinput ioctl(UI_SET_*) failed";
        close(fd);
        return;
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1209;
    setup.id.product = 0xa591;
    strncpy(setup.name, "Aspia Wakeup", sizeof(setup.name) - 1);

    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0)
    {
        PLOG(ERROR) << "Unable to create uinput device";
        close(fd);
        return;
    }

    usleep(kUinputSettleMs * 1000);

    auto emitEvent = [fd](quint16 type, quint16 code, qint32 value)
    {
        struct input_event event;
        memset(&event, 0, sizeof(event));
        event.type = type;
        event.code = code;
        event.value = value;
        if (write(fd, &event, sizeof(event)) != sizeof(event))
            PLOG(ERROR) << "Unable to write uinput event";
    };

    // Nudge the pointer and immediately move it back, so the cursor ends where it started.
    emitEvent(EV_REL, REL_X, 4);
    emitEvent(EV_REL, REL_Y, 4);
    emitEvent(EV_SYN, SYN_REPORT, 0);
    emitEvent(EV_REL, REL_X, -4);
    emitEvent(EV_REL, REL_Y, -4);
    emitEvent(EV_SYN, SYN_REPORT, 0);

    // Let the events drain before the device is destroyed.
    usleep(50 * 1000);

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
}
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
HANDLE createPowerRequest(const std::wstring_view& description)
{
    REASON_CONTEXT context;
    memset(&context, 0, sizeof(context));

    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = const_cast<wchar_t*>(description.data());

    ScopedHandle handle(PowerCreateRequest(&context));
    if (!handle.isValid())
    {
        PLOG(ERROR) << "PowerCreateRequest failed";
        return INVALID_HANDLE_VALUE;
    }

    if (!PowerSetRequest(handle, PowerRequestDisplayRequired))
        PLOG(ERROR) << "PowerSetRequest(PowerRequestDisplayRequired) failed";

    if (!PowerSetRequest(handle, PowerRequestSystemRequired))
        PLOG(ERROR) << "PowerSetRequest(PowerRequestSystemRequired) failed";

    return handle.release();
}

//--------------------------------------------------------------------------------------------------
// Takes ownership of the |handle|.
void deletePowerRequest(HANDLE handle)
{
    ScopedHandle request_handle(handle);
    if (!request_handle.isValid())
    {
        LOG(ERROR) << "Invalid handle for power request";
        return;
    }

    if (!PowerClearRequest(request_handle, PowerRequestSystemRequired))
        PLOG(ERROR) << "PowerClearRequest(PowerRequestSystemRequired) failed";

    if (!PowerClearRequest(request_handle, PowerRequestDisplayRequired))
        PLOG(ERROR) << "PowerClearRequest(PowerRequestDisplayRequired) failed";
}

//--------------------------------------------------------------------------------------------------
// The power request only prevents the display from turning off, it does not turn on a display that
// is already off. The only reliable way to wake a powered-off display is to simulate user input. A
// zero-delta mouse move resets the input idle timer and wakes the monitor without moving the cursor
// or affecting the foreground application.
void wakeUpDisplay()
{
    ScopedThreadDesktop desktop;
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop.isSame(input_desktop))
        desktop.setThreadDesktop(std::move(input_desktop));

    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type       = INPUT_MOUSE;
    input.mi.dx      = 0;
    input.mi.dy      = 0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    if (!SendInput(1, &input, sizeof(input)))
        PLOG(ERROR) << "SendInput failed";
}

#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
PowerSaveBlocker::PowerSaveBlocker()
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    : bus_(SessionDBus::connectAsActiveUser("aspia-power-save"))
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
{
    LOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    static const wchar_t kDescription[] = L"Aspia session is active";

    // The display and the system will stay on as long as the class instance exists.
    handle_.reset(createPowerRequest(kDescription));

    // The power request keeps the display on, but does not turn it back on if it is already off.
    wakeUpDisplay();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Keep the screen from blanking for the duration of the session. On Wayland a screen blank makes
    // the compositor renegotiate the screen-cast stream, which breaks capture (the stream ends with
    // "no more input formats"). org.freedesktop.ScreenSaver is implemented by both GNOME and KDE.
    QDBusInterface screen_saver(kScreenSaverService, kScreenSaverPath, kScreenSaverIface, bus_);
    QDBusReply<uint> reply = screen_saver.call("Inhibit", "Aspia", "Remote desktop session is active");
    if (reply.isValid())
        cookie_ = reply.value();
    else
        LOG(ERROR) << "ScreenSaver.Inhibit failed:" << reply.error().message();

    // Inhibit keeps the display from blanking, but does not turn it back on if it is already off.
    wakeUpDisplay();
#else
    NOTIMPLEMENTED();
#endif // defined(Q_OS_*)
}

//--------------------------------------------------------------------------------------------------
PowerSaveBlocker::~PowerSaveBlocker()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    deletePowerRequest(handle_.release());
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (bus_.isConnected())
    {
        if (cookie_)
        {
            QDBusInterface screen_saver(kScreenSaverService, kScreenSaverPath, kScreenSaverIface, bus_);
            screen_saver.call("UnInhibit", cookie_);
        }
        QDBusConnection::disconnectFromBus(bus_.name());
    }
#endif // defined(Q_OS_*)
}
