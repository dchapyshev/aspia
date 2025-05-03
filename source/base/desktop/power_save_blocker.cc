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

#include "base/desktop/power_save_blocker.h"

#include "base/logging.h"

namespace base {

namespace {

#if defined(OS_WIN)

//--------------------------------------------------------------------------------------------------
HANDLE createPowerRequest(POWER_REQUEST_TYPE type, const std::wstring_view& description)
{
    REASON_CONTEXT context;
    memset(&context, 0, sizeof(context));

    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = const_cast<wchar_t*>(description.data());

    ScopedHandle handle(PowerCreateRequest(&context));
    if (!handle.isValid())
    {
        PLOG(LS_ERROR) << "PowerCreateRequest failed";
        return INVALID_HANDLE_VALUE;
    }

    if (!PowerSetRequest(handle, type))
    {
        PLOG(LS_ERROR) << "PowerSetRequest failed";
        return INVALID_HANDLE_VALUE;
    }

    return handle.release();
}

//--------------------------------------------------------------------------------------------------
// Takes ownership of the |handle|.
void deletePowerRequest(POWER_REQUEST_TYPE type, HANDLE handle)
{
    ScopedHandle request_handle(handle);
    if (!request_handle.isValid())
    {
        LOG(LS_ERROR) << "Invalid handle for power request";
        return;
    }

    if (!PowerClearRequest(request_handle, type))
    {
        PLOG(LS_ERROR) << "PowerClearRequest failed";
    }
}

#endif // defined(OS_WIN)

} // namespace

//--------------------------------------------------------------------------------------------------
PowerSaveBlocker::PowerSaveBlocker()
{
    LOG(LS_INFO) << "Ctor";

#if defined(OS_WIN)
    static const wchar_t kDescription[] = L"Aspia session is active";

    // The display will always be on as long as the class instance exists.
    handle_.reset(createPowerRequest(PowerRequestDisplayRequired, kDescription));
#else // defined(OS_WIN)
    NOTIMPLEMENTED();
#endif // defined(OS_*)
}

//--------------------------------------------------------------------------------------------------
PowerSaveBlocker::~PowerSaveBlocker()
{
    LOG(LS_INFO) << "Dtor";

#if defined(OS_WIN)
    deletePowerRequest(PowerRequestDisplayRequired, handle_.release());
#endif // defined(OS_WIN)
}

} // namespace base
