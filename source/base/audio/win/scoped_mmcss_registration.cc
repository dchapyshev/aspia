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

#include "base/audio/win/scoped_mmcss_registration.h"

#include "base/logging.h"

#include <avrt.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
const char* priorityClassToString(DWORD priority_class)
{
    switch (priority_class)
    {
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return "ABOVE_NORMAL";
        case BELOW_NORMAL_PRIORITY_CLASS:
            return "BELOW_NORMAL";
        case HIGH_PRIORITY_CLASS:
            return "HIGH";
        case IDLE_PRIORITY_CLASS:
            return "IDLE";
        case NORMAL_PRIORITY_CLASS:
            return "NORMAL";
        case REALTIME_PRIORITY_CLASS:
            return "REALTIME";
        default:
            return "INVALID";
    }
}

//--------------------------------------------------------------------------------------------------
const char* priorityToString(int priority)
{
    switch (priority)
    {
        case THREAD_PRIORITY_ABOVE_NORMAL:
            return "ABOVE_NORMAL";
        case THREAD_PRIORITY_BELOW_NORMAL:
            return "BELOW_NORMAL";
        case THREAD_PRIORITY_HIGHEST:
            return "HIGHEST";
        case THREAD_PRIORITY_IDLE:
            return "IDLE";
        case THREAD_PRIORITY_LOWEST:
            return "LOWEST";
        case THREAD_PRIORITY_NORMAL:
            return "NORMAL";
        case THREAD_PRIORITY_TIME_CRITICAL:
            return "TIME_CRITICAL";
        default:
            // Can happen in combination with REALTIME_PRIORITY_CLASS.
            return "INVALID";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScopedMMCSSRegistration::ScopedMMCSSRegistration(const wchar_t* task_name)
{
    // Register the calling thread with MMCSS for the supplied |task_name|.
    DWORD mmcss_task_index = 0;
    mmcss_handle_ = AvSetMmThreadCharacteristicsW(task_name, &mmcss_task_index);
    if (mmcss_handle_ == nullptr)
    {
        PLOG(ERROR) << "Failed to enable MMCSS on this thread";
    }
    else
    {
        const DWORD priority_class = GetPriorityClass(GetCurrentProcess());
        const int priority = GetThreadPriority(GetCurrentThread());
        LOG(INFO) << "priority class:"
            << priorityClassToString(priority_class) << "("
            << priority_class << ")";
        LOG(INFO) << "priority:" << priorityToString(priority) << "(" << priority << ")";
    }
}

//--------------------------------------------------------------------------------------------------
ScopedMMCSSRegistration::~ScopedMMCSSRegistration()
{
    if (isSucceeded())
    {
        // Deregister with MMCSS.
        AvRevertMmThreadCharacteristics(mmcss_handle_);
    }
}

//--------------------------------------------------------------------------------------------------
bool ScopedMMCSSRegistration::isSucceeded() const
{
    return mmcss_handle_ != nullptr;
}

} // namespace base
