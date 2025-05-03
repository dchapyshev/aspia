//
// Aspia Project
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

#ifndef BASE_WIN_DESKTOP_H
#define BASE_WIN_DESKTOP_H

#include "base/macros_magic.h"

#include <qt_windows.h>

#include <string>
#include <vector>

namespace base {

class Desktop
{
public:
    Desktop() = default;
    Desktop(Desktop&& other) noexcept;
    ~Desktop();

    // Returns the desktop currently receiving user input or NULL if an error occurs.
    static Desktop inputDesktop();

    // Returns the desktop by its name or NULL if an error occurs.
    static Desktop desktop(const wchar_t* desktop_name);

    // Returns the desktop currently assigned to the calling thread or NULL if an error occurs.
    static Desktop threadDesktop();

    // Returns list of desktops in system.
    static std::vector<std::wstring> desktopList(HWINSTA winsta = nullptr);

    // Returns the name of the desktop represented by the object. Return false if quering the name
    // failed for any reason.
    bool name(wchar_t* name, DWORD length) const;

    // Returns true if |other| has the same name as this desktop. Returns false in any other case
    // including failing Win32 APIs and uninitialized desktop handles.
    bool isSame(const Desktop& other) const;

    // Assigns the desktop to the current thread. Returns false is the operation failed for any
    // reason.
    bool setThreadDesktop() const;

    bool isValid() const;

    void close();

    Desktop& operator=(Desktop&& other) noexcept;

private:
    Desktop(HDESK desktop, bool own);

    static BOOL CALLBACK enumDesktopProc(LPWSTR desktop, LPARAM lparam);

    // The desktop handle.
    HDESK desktop_ = nullptr;

    // True if |desktop_| must be closed on teardown.
    bool own_ = false;

    DISALLOW_COPY_AND_ASSIGN(Desktop);
};

} // namespace base

#endif // BASE_WIN_DESKTOP_H
