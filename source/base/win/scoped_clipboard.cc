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

#include "base/win/scoped_clipboard.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
bool ScopedClipboard::init(HWND owner)
{
    const int kMaxAttemptsToOpenClipboard = 5;
    const DWORD kSleepTimeBetweenAttempts = 5;

    if (opened_)
    {
        LOG(ERROR) << "Attempt to open an already opened clipboard";
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

//--------------------------------------------------------------------------------------------------
BOOL ScopedClipboard::empty()
{
    if (!opened_)
    {
        LOG(ERROR) << "Clipboard is not open";
        return FALSE;
    }

    return EmptyClipboard();
}

//--------------------------------------------------------------------------------------------------
void ScopedClipboard::setData(UINT format, HANDLE mem)
{
    if (!opened_)
    {
        LOG(ERROR) << "Clipboard is not open";
        return;
    }

    // The caller must not close the handle that SetClipboardData returns.
    SetClipboardData(format, mem);
}

//--------------------------------------------------------------------------------------------------
HANDLE ScopedClipboard::data(UINT format) const
{
    if (!opened_)
    {
        LOG(ERROR) << "Clipboard is not open";
        return nullptr;
    }

    return GetClipboardData(format);
}

} // namespace base
