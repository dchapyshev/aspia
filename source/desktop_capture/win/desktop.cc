//
// PROJECT:         Aspia
// FILE:            desktop_capture/win/desktop.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/win/desktop.h"

#include <QDebug>

#include "base/errno_logging.h"

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
    close();
}

// static
Desktop Desktop::desktop(const wchar_t* desktop_name)
{
    const ACCESS_MASK desired_access =
        DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
        DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
        DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | GENERIC_WRITE;

    HDESK desktop = OpenDesktopW(desktop_name, 0, FALSE, desired_access);
    if (!desktop)
    {
        qDebugErrno("OpenDesktopW failed");
        return Desktop();
    }

    return Desktop(desktop, true);
}

// static
Desktop Desktop::inputDesktop()
{
    const ACCESS_MASK desired_access = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;

    HDESK desktop = OpenInputDesktop(0, FALSE, desired_access);
    if (!desktop)
    {
        qDebugErrno("OpenInputDesktop failed");
        return Desktop();
    }

    return Desktop(desktop, true);
}

// static
Desktop Desktop::threadDesktop()
{
    HDESK desktop = GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        qDebugErrno("GetThreadDesktop failed");
        return Desktop();
    }

    return Desktop(desktop, false);
}

bool Desktop::name(wchar_t* name, DWORD length) const
{
    if (!desktop_)
        return false;

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        qDebugErrno("Failed to query the desktop name");
        return false;
    }

    return true;
}

bool Desktop::isSame(const Desktop& other) const
{
    wchar_t this_name[128];

    if (!name(this_name, sizeof(this_name)))
        return false;

    wchar_t other_name[128];

    if (!other.name(other_name, sizeof(other_name)))
        return false;

    return wcscmp(this_name, other_name) == 0;
}

bool Desktop::setThreadDesktop() const
{
    if (!SetThreadDesktop(desktop_))
    {
        qDebugErrno("SetThreadDesktop failed");
        return false;
    }

    return true;
}

bool Desktop::isValid() const
{
    return (desktop_ != nullptr);
}

void Desktop::close()
{
    if (own_ && desktop_)
    {
        if (!CloseDesktop(desktop_))
        {
            qDebugErrno("CloseDesktop failed");
        }
    }

    desktop_ = nullptr;
}

Desktop& Desktop::operator=(Desktop&& other) noexcept
{
    close();

    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;

    return *this;
}

} // namespace aspia
