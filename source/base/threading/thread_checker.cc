//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/threading/thread_checker.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/threading/thread_checker.h"

namespace aspia {

ThreadChecker::ThreadChecker()
{
    EnsureAssigned();
}

bool ThreadChecker::CalledOnValidThread() const
{
    std::lock_guard<std::mutex> lock(thread_id_lock_);
    if (thread_id_ == std::this_thread::get_id())
        return true;

    return false;
}

void ThreadChecker::DetachFromThread()
{
    std::lock_guard<std::mutex> lock(thread_id_lock_);
    thread_id_ = std::thread::id();
}

void ThreadChecker::EnsureAssigned()
{
    std::lock_guard<std::mutex> lock(thread_id_lock_);
    thread_id_ = std::this_thread::get_id();
}

} // namespace aspia
