//
// PROJECT:         Aspia
// FILE:            desktop_capture/scoped_thread_desktop.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__SCOPED_THREAD_DESKTOP_H
#define _ASPIA_DESKTOP_CAPTURE__SCOPED_THREAD_DESKTOP_H

#include "base/macros.h"
#include "desktop_capture/desktop.h"

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
    bool isSame(const Desktop& desktop) const;

    // Reverts the calling thread to use the initial desktop.
    void revert();

    //
    // Assigns |desktop| to be the calling thread. Returns true if the thread has
    // been switched to |desktop| successfully. Takes ownership of |desktop|.
    //
    bool setThreadDesktop(Desktop desktop);

private:
    // The desktop handle assigned to the calling thread by Set
    Desktop assigned_;

    // The desktop handle assigned to the calling thread at creation.
    Desktop initial_;

    DISALLOW_COPY_AND_ASSIGN(ScopedThreadDesktop);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__SCOPED_THREAD_DESKTOP_H
