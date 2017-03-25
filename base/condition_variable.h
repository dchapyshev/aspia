//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/condition_variable.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CONDITION_VARIABLE_H
#define _ASPIA_BASE__CONDITION_VARIABLE_H

#include "base/lock.h"

namespace aspia {

class ConditionVariable
{
public:
    explicit ConditionVariable(Lock* user_lock);
    ~ConditionVariable() = default;

    void Wait(uint32_t timeout = INFINITE);
    void Broadcast();
    void Signal();

private:
    CONDITION_VARIABLE cv_;
    SRWLOCK *const srwlock_;

    DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

} // namespace aspia

#endif // _ASPIA_BASE__CONDITION_VARIABLE_H
