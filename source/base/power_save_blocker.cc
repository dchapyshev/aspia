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

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QTimer>

#include <linux/uinput.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#include "base/logging.h"
#include "base/time_types.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_thread_desktop.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_MACOS)
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif // defined(Q_OS_MACOS)

namespace {

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
// Delay before the first nudge, to let the compositor's input stack enumerate the new uinput device
// (via udev/logind) - a nudge emitted before anyone reads the device is lost.
const Seconds kInitialWakeDelay{ 1 };

// Interval of the repeating keep-awake nudge, used only where the screen-saver inhibit is unavailable
// (e.g. GNOME). Shorter than any practical blank timeout so the display never idles off.
const Seconds kWakeInterval{ 30 };

//--------------------------------------------------------------------------------------------------
// Creates a persistent virtual pointer via uinput. Returns its fd, or -1 on failure. The device must
// outlive a single nudge: a throwaway one is destroyed before the compositor's input stack enumerates
// it, so its events never reach anyone. Requires write access to /dev/uinput (i.e. running as root).
int createWakeDevice()
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
    {
        PLOG(ERROR) << "Unable to open /dev/uinput";
        return -1;
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
        return -1;
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
        return -1;
    }

    return fd;
}

//--------------------------------------------------------------------------------------------------
// Feeds a net-zero pointer motion through |fd|. The kernel input event resets the idle timer and
// wakes a DPMS-off display regardless of the display server; the move cancels itself so the cursor
// stays put.
void emitWakeNudge(int fd)
{
    if (fd < 0)
        return;

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

    emitEvent(EV_REL, REL_X, 1);
    emitEvent(EV_REL, REL_Y, 1);
    emitEvent(EV_SYN, SYN_REPORT, 0);
    emitEvent(EV_REL, REL_X, -1);
    emitEvent(EV_REL, REL_Y, -1);
    emitEvent(EV_SYN, SYN_REPORT, 0);
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
PowerSaveBlocker::PowerSaveBlocker(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    static const wchar_t kDescription[] = L"Aspia session is active";

    // The display and the system will stay on as long as the class instance exists.
    handle_.reset(createPowerRequest(kDescription));

    // The power request keeps the display on, but does not turn it back on if it is already off.
    wakeUpDisplay();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Keep the screen from blanking for the duration of the session (on Wayland a blank makes the
    // compositor renegotiate the screen-cast stream and breaks capture; with KMS a blanked CRTC has no
    // scan-out to read at all). A screen-saver inhibit over D-Bus is unreliable (GNOME does not provide
    // org.freedesktop.ScreenSaver) and cannot wake an already-off display, so feed input through a
    // persistent uinput device instead: it works on every compositor and X11 alike.
    uinput_fd_ = createWakeDevice();
    if (uinput_fd_ >= 0)
    {
        // Keep the display awake for the whole session with a periodic nudge.
        wake_timer_ = new QTimer(this);
        wake_timer_->setInterval(kWakeInterval);
        connect(wake_timer_, &QTimer::timeout, this, &PowerSaveBlocker::onWakeUp);
        wake_timer_->start();

        // Wake it now (if already off) once the compositor's input stack has had time to enumerate the
        // device - a nudge sent before that is read by no one and lost.
        QTimer::singleShot(kInitialWakeDelay, this, &PowerSaveBlocker::onWakeUp);
    }
#elif defined(Q_OS_MACOS)
    const CFStringRef reason = CFSTR("Aspia session is active");

    // Keep the display and the system from idle-sleeping for as long as this instance exists.
    IOPMAssertionID display_id = kIOPMNullAssertionID;
    if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                    kIOPMAssertionLevelOn, reason, &display_id) == kIOReturnSuccess)
        display_assertion_ = display_id;
    else
        LOG(ERROR) << "IOPMAssertionCreateWithName (display) failed";

    IOPMAssertionID system_id = kIOPMNullAssertionID;
    if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,
                                    kIOPMAssertionLevelOn, reason, &system_id) == kIOReturnSuccess)
        system_assertion_ = system_id;
    else
        LOG(ERROR) << "IOPMAssertionCreateWithName (system) failed";

    // The assertions keep an on display on, but do not wake one that is already asleep. Declaring
    // user activity wakes it (mirrors the synthetic input used on Windows and Linux).
    IOPMAssertionID activity_id = kIOPMNullAssertionID;
    if (IOPMAssertionDeclareUserActivity(reason, kIOPMUserActiveLocal, &activity_id) ==
            kIOReturnSuccess)
        activity_assertion_ = activity_id;
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
#elif defined(Q_OS_MACOS)
    if (activity_assertion_)
        IOPMAssertionRelease(activity_assertion_);
    if (system_assertion_)
        IOPMAssertionRelease(system_assertion_);
    if (display_assertion_)
        IOPMAssertionRelease(display_assertion_);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // |wake_timer_| is a child object and is destroyed with this one.
    if (uinput_fd_ >= 0)
    {
        ioctl(uinput_fd_, UI_DEV_DESTROY);
        close(uinput_fd_);
        uinput_fd_ = -1;
    }
#endif // defined(Q_OS_*)
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
//--------------------------------------------------------------------------------------------------
void PowerSaveBlocker::onWakeUp()
{
    emitWakeNudge(uinput_fd_);
}
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
