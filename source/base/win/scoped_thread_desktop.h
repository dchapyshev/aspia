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

#ifndef BASE_WIN_SCOPED_THREAD_DESKTOP_H
#define BASE_WIN_SCOPED_THREAD_DESKTOP_H

#include "base/win/desktop.h"

namespace base {

class ScopedThreadDesktop
{
public:
    ScopedThreadDesktop();
    ~ScopedThreadDesktop();

    // Returns true if |desktop| has the same desktop name as the currently assigned desktop (if
    // assigned) or as the initial desktop (if not assigned).
    // Returns false in any other case including failing Win32 APIs and uninitialized desktop handles.
    bool isSame(const Desktop& desktop) const;

    // Reverts the calling thread to use the initial desktop.
    void revert();

    // Assigns |desktop| to be the calling thread. Returns true if the thread has been switched to
    // |desktop| successfully. Takes ownership of |desktop|.
    bool setThreadDesktop(Desktop&& desktop);

    const Desktop& assignedDesktop() const { return assigned_; }
    const Desktop& initialDesktop() const { return initial_; }

private:
    // The desktop handle assigned to the calling thread by |setThreadDesktop|.
    Desktop assigned_;

    // The desktop handle assigned to the calling thread at creation.
    Desktop initial_;

    Q_DISABLE_COPY(ScopedThreadDesktop)
};

} // namespace base

#endif // BASE_WIN_SCOPED_THREAD_DESKTOP_H
