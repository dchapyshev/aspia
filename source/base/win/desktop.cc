//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/desktop.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Desktop::Desktop(Desktop&& other) noexcept
{
    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
Desktop::Desktop(HDESK desktop, bool own)
    : desktop_(desktop),
      own_(own)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Desktop::~Desktop()
{
    close();
}

//--------------------------------------------------------------------------------------------------
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
        PLOG(ERROR) << "OpenDesktopW failed";
        return Desktop();
    }

    return Desktop(desktop, true);
}

//--------------------------------------------------------------------------------------------------
// static
Desktop Desktop::inputDesktop()
{
    const ACCESS_MASK desired_access = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;

    HDESK desktop = OpenInputDesktop(0, FALSE, desired_access);
    if (!desktop)
    {
        PLOG(ERROR) << "OpenInputDesktop failed";
        return Desktop();
    }

    return Desktop(desktop, true);
}

//--------------------------------------------------------------------------------------------------
// static
Desktop Desktop::threadDesktop()
{
    HDESK desktop = GetThreadDesktop(GetCurrentThreadId());
    if (!desktop)
    {
        PLOG(ERROR) << "GetThreadDesktop failed";
        return Desktop();
    }

    return Desktop(desktop, false);
}

//--------------------------------------------------------------------------------------------------
// static
QStringList Desktop::desktopList(HWINSTA winsta)
{
    QStringList list;

    if (!EnumDesktopsW(winsta, enumDesktopProc, reinterpret_cast<LPARAM>(&list)))
    {
        PLOG(ERROR) << "EnumDesktopsW failed";
        return {};
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
bool Desktop::name(wchar_t* name, DWORD length) const
{
    if (!desktop_)
        return false;

    if (!GetUserObjectInformationW(desktop_, UOI_NAME, name, length, nullptr))
    {
        PLOG(ERROR) << "Failed to query the desktop name";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
bool Desktop::setThreadDesktop() const
{
    if (!SetThreadDesktop(desktop_))
    {
        PLOG(ERROR) << "SetThreadDesktop failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Desktop::isValid() const
{
    return (desktop_ != nullptr);
}

//--------------------------------------------------------------------------------------------------
void Desktop::close()
{
    if (own_ && desktop_)
    {
        if (!CloseDesktop(desktop_))
        {
            PLOG(ERROR) << "CloseDesktop failed";
        }
    }

    desktop_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
Desktop& Desktop::operator=(Desktop&& other) noexcept
{
    close();

    desktop_ = other.desktop_;
    own_ = other.own_;

    other.desktop_ = nullptr;

    return *this;
}

//--------------------------------------------------------------------------------------------------
// static
BOOL CALLBACK Desktop::enumDesktopProc(LPWSTR desktop, LPARAM lparam)
{
    QStringList* list = reinterpret_cast<QStringList*>(lparam);
    if (!list)
    {
        LOG(ERROR) << "Invalid desktop list pointer";
        return FALSE;
    }

    if (!desktop)
    {
        LOG(ERROR) << "Invalid desktop name";
        return FALSE;
    }

    list->emplace_back(QString::fromWCharArray(desktop));
    return TRUE;
}

} // namespace base
