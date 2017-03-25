//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/desktop.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/desktop.h"
#include "base/logging.h"

namespace aspia {

Desktop::Desktop() :
    desktop_(nullptr),
    own_(false)
{
    // Nothing
}

Desktop::Desktop(Desktop&& other)
{
    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;
}

Desktop::Desktop(HDESK desktop, bool own) :
    desktop_(desktop),
    own_(own)
{
    // Nothing
}

Desktop::~Desktop()
{
    Close();
}

// static
Desktop Desktop::GetDesktop(const WCHAR* desktop_name)
{
    const ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
        DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
        DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;

    HDESK desktop = OpenDesktopW(desktop_name, 0, FALSE, desired_access);
    if (!desktop)
    {
        DLOG(ERROR) << "OpenDesktopW() failed: " << GetLastError();
        return Desktop(nullptr, false);
    }

    return Desktop(desktop, true);
}

// static
Desktop Desktop::GetInputDesktop()
{
    const ACCESS_MASK desired_access = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;

    HDESK desktop = OpenInputDesktop(0, FALSE, desired_access);
    if (!desktop)
    {
        DLOG(ERROR) << "OpenInputDesktop() failed: " << GetLastError();
        return Desktop(nullptr, false);
    }

    return Desktop(desktop, true);
}

// static
Desktop Desktop::GetThreadDesktop()
{
    HDESK desktop = ::GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        DLOG(ERROR) << "GetThreadDesktop() failed: " << GetLastError();
        return Desktop(nullptr, false);
    }

    return Desktop(desktop, false);
}

bool Desktop::GetName(WCHAR* name, DWORD length) const
{
    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        DLOG(ERROR) << "Failed to query the desktop name: " << GetLastError();
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
        DLOG(ERROR) << "SetThreadDesktop() failed: " << GetLastError();
        return false;
    }

    return true;
}

bool Desktop::IsValid() const
{
    return (desktop_ != nullptr);
}

void Desktop::Close()
{
    if (own_ && desktop_)
    {
        if (!CloseDesktop(desktop_))
        {
            DLOG(ERROR) << "CloseDesktop() failed: " << GetLastError();
        }
    }

    desktop_ = nullptr;
}

Desktop& Desktop::operator=(Desktop&& other)
{
    Close();

    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;

    return *this;
}

} // namespace aspia
