//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_thread_desktop.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_THREAD_DESKTOP_H
#define _ASPIA_BASE__SCOPED_THREAD_DESKTOP_H

#include <memory>

#include "base/macros.h"
#include "base/desktop.h"

namespace aspia {

class ScopedThreadDesktop
{
public:
    ScopedThreadDesktop();
    ~ScopedThreadDesktop();

    //
    // Returns true if |desktop| has the same desktop name as the currently
    // assigned desktop (if assigned) or as the initial desktop (if not assigned).
    // Returns false in any other case including failing Win32 APIs and
    // uninitialized desktop handles.
    //
    bool IsSame(const Desktop& desktop);

    // Reverts the calling thread to use the initial desktop.
    void Revert();

    //
    // Assigns |desktop| to be the calling thread. Returns true if the thread has
    // been switched to |desktop| successfully. Takes ownership of |desktop|.
    //
    bool SetThreadDesktop(Desktop desktop);

private:
    // The desktop handle assigned to the calling thread by Set
    Desktop assigned_;

    // The desktop handle assigned to the calling thread at creation.
    Desktop initial_;

    DISALLOW_COPY_AND_ASSIGN(ScopedThreadDesktop);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_THREAD_DESKTOP_H
