//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QThread>

#include "base/logging.h"
#include "base/time_types.h"

//--------------------------------------------------------------------------------------------------
ScopedClipboard::~ScopedClipboard()
{
    if (opened_)
    {
        // CloseClipboard() must be called with anonymous access token. See
        // crbug.com/441834.
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
    const MilliSeconds kSleepTimeBetweenAttempts{ 5 };

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
            QThread::sleep(kSleepTimeBetweenAttempts);
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
        GlobalFree(mem);
        return;
    }

    if (!SetClipboardData(format, mem))
    {
        PLOG(ERROR) << "SetClipboardData failed";
        GlobalFree(mem);
    }
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
