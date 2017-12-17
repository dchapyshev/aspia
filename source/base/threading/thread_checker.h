//
// PROJECT:         Aspia
// FILE:            base/threading/thread_checker.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__THREADING__THREAD_CHECKER_H
#define _ASPIA_BASE__THREADING__THREAD_CHECKER_H

#include "base/logging.h"
#include "base/macros.h"

#include <thread>
#include <mutex>

// ThreadChecker is a helper class used to help verify that some methods of a
// class are called from the same thread (for thread-affinity).
//
// Use the macros below instead of the ThreadChecker directly so that the unused
// member doesn't result in an extra byte (four when padded) per instance in
// production.
//
// Usage:
//   class MyClass {
//    public:
//     MyClass() {
//       // It's sometimes useful to detach on construction for objects that are
//       // constructed in one place and forever after used from another
//       // thread.
//       DETACH_FROM_THREAD(my_thread_checker_);
//     }
//
//     ~MyClass() {
//       // ThreadChecker doesn't automatically check it's destroyed on origin
//       // thread for the same reason it's sometimes detached in the
//       // constructor. It's okay to destroy off thread if the owner otherwise
//       // knows usage on the associated thread is done. If you're not
//       // detaching in the constructor, you probably want to explicitly check
//       // in the destructor.
//       DCHECK_CALLED_ON_VALID_THREAD(my_thread_checker_);
//     }
//
//     void MyMethod() {
//       DCHECK_CALLED_ON_VALID_THREAD(my_thread_checker_);
//       ... (do stuff) ...
//     }
//
//    private:
//     THREAD_CHECKER(my_thread_checker_);
//   }

namespace aspia {

class ThreadChecker
{
public:
    ThreadChecker();
    ~ThreadChecker() = default;

    bool CalledOnValidThread() const;
    void DetachFromThread();

private:
    void EnsureAssigned();

    std::thread::id thread_id_;
    mutable std::mutex thread_id_lock_;

    DISALLOW_COPY_AND_ASSIGN(ThreadChecker);
};

} // namespace aspia

#ifndef NDEBUG
#define THREAD_CHECKER(name) aspia::ThreadChecker name
#define DCHECK_CALLED_ON_VALID_THREAD(name) DCHECK((name).CalledOnValidThread())
#define DETACH_FROM_THREAD(name) (name).DetachFromThread()
#else // NDEBUG
#define THREAD_CHECKER(name)
#define DCHECK_CALLED_ON_VALID_THREAD(name)
#define DETACH_FROM_THREAD(name)
#endif // NDEBUG

#endif // _ASPIA_BASE__THREADING__THREAD_CHECKER_H
