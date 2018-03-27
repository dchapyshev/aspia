//
// PROJECT:         Aspia
// FILE:            base/win/scoped_clipboard.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_CLIPBOARD_H
#define _ASPIA_BASE__WIN__SCOPED_CLIPBOARD_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QtGlobal>

namespace aspia {

class ScopedClipboard
{
public:
    ScopedClipboard() = default;
    ~ScopedClipboard();

    bool Init(HWND owner);
    BOOL Empty();
    void SetData(UINT format, HANDLE mem);

    //
    // The caller must not free the handle. The caller should lock the handle,
    // copy the clipboard data, and unlock the handle. All this must be done
    // before this ScopedClipboard is destroyed.
    //
    HANDLE GetData(UINT format) const;

private:
    bool opened_ = false;

    Q_DISABLE_COPY(ScopedClipboard)
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_CLIPBOARD_H
