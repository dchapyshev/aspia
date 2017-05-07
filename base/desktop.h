//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/desktop.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DESKTOP_H
#define _ASPIA_BASE__DESKTOP_H

#include "base/macros.h"

namespace aspia {

class Desktop
{
public:
    Desktop();
    Desktop(Desktop&& other);
    ~Desktop();

    //
    // Returns the desktop currently receiving user input or NULL if an error
    // occurs.
    //
    static Desktop GetInputDesktop();

    //
    // Returns the desktop by its name or NULL if an error occurs.
    //
    static Desktop GetDesktop(const WCHAR* desktop_name);

    //
    // Returns the desktop currently assigned to the calling thread or NULL if
    // an error occurs.
    //
    static Desktop GetThreadDesktop();

    //
    // Returns the name of the desktop represented by the object. Return false if
    // quering the name failed for any reason.
    //
    bool GetName(WCHAR* name, DWORD length) const;

    //
    // Returns true if |other| has the same name as this desktop. Returns false
    // in any other case including failing Win32 APIs and uninitialized desktop
    // handles.
    //
    bool IsSame(const Desktop& other) const;

    //
    // Assigns the desktop to the current thread. Returns false is the operation
    // failed for any reason.
    //
    bool SetThreadDesktop() const;

    bool IsValid() const;

    void Close();

    Desktop& operator=(Desktop&& other);

private:
    Desktop(HDESK desktop, bool own);

private:
    // The desktop handle.
    HDESK desktop_;

    // True if |desktop_| must be closed on teardown.
    bool own_;

    DISALLOW_COPY_AND_ASSIGN(Desktop);
};

} // namespace aspia

#endif // _ASPIA_BASE__DESKTOP_H
