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

#include "base/desktop/power_save_blocker.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_thread_desktop.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

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
{
    LOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    static const wchar_t kDescription[] = L"Aspia session is active";

    // The display and the system will stay on as long as the class instance exists.
    handle_.reset(createPowerRequest(kDescription));

    // The power request keeps the display on, but does not turn it back on if it is already off.
    wakeUpDisplay();
#else // defined(Q_OS_WINDOWS)
    NOTIMPLEMENTED();
#endif // defined(Q_OS_*)
}

//--------------------------------------------------------------------------------------------------
PowerSaveBlocker::~PowerSaveBlocker()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    deletePowerRequest(handle_.release());
#endif // defined(Q_OS_WINDOWS)
}
