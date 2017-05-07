//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_clipboard.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_clipboard.h"

#include "base/logging.h"

namespace aspia {

ScopedClipboard::ScopedClipboard() :
    opened_(false)
{
    // Nothing
}

ScopedClipboard::~ScopedClipboard()
{
    if (opened_)
    {
        //
        // CloseClipboard() must be called with anonymous access token. See
        // crbug.com/441834.
        //
        BOOL result = ImpersonateAnonymousToken(GetCurrentThread());
        CHECK(result);

        CloseClipboard();

        result = RevertToSelf();
        CHECK(result);
    }
}

bool ScopedClipboard::Init(HWND owner)
{
    const int kMaxAttemptsToOpenClipboard = 5;
    const DWORD kSleepTimeBetweenAttempts = 5;

    if (opened_)
    {
        DLOG(ERROR) << "Attempt to open an already opened clipboard";
        return true;
    }

    // This code runs on the UI thread, so we can block only very briefly.
    for (int attempt = 0; attempt < kMaxAttemptsToOpenClipboard; ++attempt)
    {
        if (attempt > 0)
        {
            Sleep(kSleepTimeBetweenAttempts);
        }

        if (OpenClipboard(owner))
        {
            opened_ = true;
            return true;
        }
    }

    return false;
}

BOOL ScopedClipboard::Empty()
{
    if (!opened_)
    {
        DLOG(WARNING) << "Clipboard is not open";
        return FALSE;
    }

    return EmptyClipboard();
}

void ScopedClipboard::SetData(UINT format, HANDLE mem)
{
    if (!opened_)
    {
        DLOG(WARNING) << "Clipboard is not open";
        return;
    }

    // The caller must not close the handle that SetClipboardData returns.
    SetClipboardData(format, mem);
}

HANDLE ScopedClipboard::GetData(UINT format)
{
    if (!opened_)
    {
        DLOG(WARNING) << "Clipboard is not open";
        return nullptr;
    }

    return GetClipboardData(format);
}

} // namespace aspia
