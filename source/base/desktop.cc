//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/desktop.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/desktop.h"
#include "base/logging.h"

namespace aspia {

Desktop::Desktop(HDESK desktop, bool own)
    : desktop_(desktop), own_(own)
{
    // Nothing
}

Desktop::~Desktop()
{
    if (own_ && !desktop_)
    {
        if (!CloseDesktop(desktop_))
        {
            LOG(ERROR) << "Failed to close the owned desktop handle: "
                       << GetLastSystemErrorString();
        }
    }
}

bool Desktop::GetName(WCHAR* name, DWORD length) const
{
    if (!desktop_)
        return false;

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        LOG(ERROR) << "Failed to query the desktop name: "
                   << GetLastSystemErrorString();
        return false;
    }

    return true;
}

bool Desktop::IsSame(const Desktop& other) const
{
    WCHAR name[128];

    if (!GetName(name, sizeof(name)))
        return false;

    WCHAR other_name[128];

    if (!other.GetName(other_name, sizeof(other_name)))
        return false;

    return wcscmp(name, other_name) == 0;
}

bool Desktop::SetThreadDesktop() const
{
    if (!::SetThreadDesktop(desktop_))
    {
        LOG(ERROR) << "Failed to assign the desktop to the current thread: "
                   << GetLastSystemErrorString();
        return false;
    }

    return true;
}

Desktop* Desktop::GetDesktop(const WCHAR* desktop_name)
{
    ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE |
        DESKTOP_HOOKCONTROL | DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;
    HDESK desktop = OpenDesktop(desktop_name, 0, FALSE, desired_access);
    if (!desktop)
    {
        LOG(ERROR) << "Failed to open the desktop '" << desktop_name << "': "
                   << GetLastSystemErrorString();
        return nullptr;
    }

    return new Desktop(desktop, true);
}

Desktop* Desktop::GetInputDesktop()
{
    HDESK desktop = OpenInputDesktop(
        0, FALSE, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE);
    if (!desktop)
        return nullptr;

    return new Desktop(desktop, true);
}

Desktop* Desktop::GetThreadDesktop()
{
    HDESK desktop = ::GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        LOG(ERROR) << "Failed to retrieve the handle of the desktop assigned to "
                      "the current thread: "
                   << GetLastSystemErrorString();
        return nullptr;
    }

    return new Desktop(desktop, false);
}

} // namespace aspia
