//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/desktop.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/desktop.h"

#include "base/logging.h"

namespace aspia {

Desktop::Desktop(HDESK desktop, bool own) :
    desktop_(desktop),
    own_(own)
{
    // Nothing
}

Desktop::~Desktop()
{
    if (own_ && desktop_)
    {
        if (!CloseDesktop(desktop_))
        {
            DLOG(ERROR) << "CloseDesktop() failed: " << GetLastError();
        }
    }
}

// static
Desktop* Desktop::GetDesktop(const WCHAR *desktop_name)
{
    ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
        DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
        DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;

    HDESK desktop = OpenDesktopW(desktop_name, 0, FALSE, desired_access);
    if (!desktop)
    {
        LOG(ERROR) << "OpenDesktopW() failed: " << GetLastError();
        return nullptr;
    }

    return new Desktop(desktop, true);
}

// static
Desktop* Desktop::GetInputDesktop()
{
    ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
        DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
        DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;

    HDESK desktop = OpenInputDesktop(0, FALSE, desired_access);
    if (!desktop)
    {
        LOG(ERROR) << "OpenInputDesktop() failed: " << GetLastError();
        return nullptr;
    }

    return new Desktop(desktop, true);
}

// static
Desktop* Desktop::GetThreadDesktop()
{
    HDESK desktop = ::GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        LOG(ERROR) << "GetThreadDesktop() failed: " << GetLastError();
        return nullptr;
    }

    return new Desktop(desktop, false);
}

bool Desktop::GetName(WCHAR *name, DWORD length) const
{
    if (!desktop_)
        return false;

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        LOG(ERROR) << "Failed to query the desktop name: " << GetLastError();
        return false;
    }

    return true;
}

bool Desktop::IsSame(const Desktop &other) const
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
        LOG(ERROR) << "SetThreadDesktop() failed: " << GetLastError();
        return false;
    }

    return true;
}

} // namespace aspia
