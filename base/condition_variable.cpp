//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/condition_variable.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/condition_variable.h"
#include "base/logging.h"

namespace aspia {

ConditionVariable::ConditionVariable(Lock* user_lock) :
    srwlock_(user_lock->native_handle())
{
    DCHECK(user_lock);
    InitializeConditionVariable(&cv_);
}

void ConditionVariable::Wait(uint32_t timeout)
{
    if (!SleepConditionVariableSRW(&cv_, srwlock_, timeout, 0))
    {
        DCHECK_EQ(static_cast<DWORD>(ERROR_TIMEOUT), GetLastError());
    }
}

void ConditionVariable::Broadcast()
{
    WakeAllConditionVariable(&cv_);
}

void ConditionVariable::Signal()
{
    WakeConditionVariable(&cv_);
}

} // namespace aspia
