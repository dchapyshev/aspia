//
// PROJECT:         Aspia
// FILE:            base/desktop.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/desktop.h"
#include "base/logging.h"

namespace aspia {

Desktop::Desktop(Desktop&& other) noexcept
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
Desktop Desktop::GetDesktop(const wchar_t* desktop_name)
{
    const ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
        DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
        DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;

    HDESK desktop = OpenDesktopW(desktop_name, 0, FALSE, desired_access);
    if (!desktop)
    {
        DPLOG(LS_ERROR) << "OpenDesktopW failed";
        return Desktop();
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
        DPLOG(LS_ERROR) << "OpenInputDesktop failed";
        return Desktop();
    }

    return Desktop(desktop, true);
}

// static
Desktop Desktop::GetThreadDesktop()
{
    HDESK desktop = ::GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        DPLOG(LS_ERROR) << "GetThreadDesktop failed";
        return Desktop();
    }

    return Desktop(desktop, false);
}

bool Desktop::GetName(wchar_t* name, DWORD length) const
{
    if (!desktop_)
        return false;

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        DPLOG(LS_ERROR) << "Failed to query the desktop name";
        return false;
    }

    return true;
}

bool Desktop::IsSame(const Desktop& other) const
{
    wchar_t name[128];

    if (!GetName(name, sizeof(name)))
        return false;

    wchar_t other_name[128];

    if (!other.GetName(other_name, sizeof(other_name)))
        return false;

    return wcscmp(name, other_name) == 0;
}

bool Desktop::SetThreadDesktop() const
{
    if (!::SetThreadDesktop(desktop_))
    {
        DPLOG(LS_ERROR) << "SetThreadDesktop failed";
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
            DPLOG(LS_ERROR) << "CloseDesktop failed";
        }
    }

    desktop_ = nullptr;
}

Desktop& Desktop::operator=(Desktop&& other) noexcept
{
    Close();

    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;

    return *this;
}

} // namespace aspia
