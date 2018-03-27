//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_H

#include <QtGlobal>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

class Desktop
{
public:
    Desktop() = default;
    Desktop(Desktop&& other) noexcept;
    ~Desktop();

    //
    // Returns the desktop currently receiving user input or NULL if an error
    // occurs.
    //
    static Desktop inputDesktop();

    //
    // Returns the desktop by its name or NULL if an error occurs.
    //
    static Desktop desktop(const wchar_t* desktop_name);

    //
    // Returns the desktop currently assigned to the calling thread or NULL if
    // an error occurs.
    //
    static Desktop threadDesktop();

    //
    // Returns the name of the desktop represented by the object. Return false if
    // quering the name failed for any reason.
    //
    bool name(wchar_t* name, DWORD length) const;

    //
    // Returns true if |other| has the same name as this desktop. Returns false
    // in any other case including failing Win32 APIs and uninitialized desktop
    // handles.
    //
    bool isSame(const Desktop& other) const;

    //
    // Assigns the desktop to the current thread. Returns false is the operation
    // failed for any reason.
    //
    bool setThreadDesktop() const;

    bool isValid() const;

    void close();

    Desktop& operator=(Desktop&& other) noexcept;

private:
    Desktop(HDESK desktop, bool own);

private:
    // The desktop handle.
    HDESK desktop_ = nullptr;

    // True if |desktop_| must be closed on teardown.
    bool own_ = false;

    Q_DISABLE_COPY(Desktop)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_H
