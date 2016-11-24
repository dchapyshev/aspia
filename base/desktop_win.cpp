/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/desktop_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/desktop_win.h"

#include "base/logging.h"

Desktop::Desktop(HDESK desktop, bool own) :
    desktop_(desktop),
    own_(own)
{
    // Ничего не делаем
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

bool Desktop::GetName(std::wstring *desktop_name_out) const
{
    if (!desktop_)
        return false;

    DWORD length = 0;

    int rv = GetUserObjectInformationW(desktop_, UOI_NAME, nullptr, 0, &length);
    if (rv || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        abort();

    length /= sizeof(WCHAR);
    std::vector<WCHAR> buffer(length);

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, &buffer[0], length * sizeof(WCHAR), &length))
    {
        LOG(ERROR) << "Failed to query the desktop name: " << GetLastError();
        return false;
    }

    desktop_name_out->assign(&buffer[0], length / sizeof(WCHAR));

    return true;
}

bool Desktop::IsSame(const Desktop &other) const
{
    std::wstring name;
    if (!GetName(&name))
        return false;

    std::wstring other_name;
    if (!other.GetName(&other_name))
        return false;

    return (name == other_name);
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
