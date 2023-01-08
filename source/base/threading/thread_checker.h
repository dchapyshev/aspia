//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_THREADING_THREAD_CHECKER_H
#define BASE_THREADING_THREAD_CHECKER_H

#include "base/macros_magic.h"

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
//   class MyClass
//   {
//   public:
//       MyClass()
//       {
//           // It's sometimes useful to detach on construction for objects that are
//           // constructed in one place and forever after used from another
//           // thread.
//           DETACH_FROM_THREAD(my_thread_checker_);
//       }
//
//       ~MyClass()
//       {
//           // ThreadChecker doesn't automatically check it's destroyed on origin
//           // thread for the same reason it's sometimes detached in the
//           // constructor. It's okay to destroy off thread if the owner otherwise
//           // knows usage on the associated thread is done. If you're not
//           // detaching in the constructor, you probably want to explicitly check
//           // in the destructor.
//           DCHECK_CALLED_ON_VALID_THREAD(my_thread_checker_);
//       }
//
//       void MyMethod()
//       {
//           DCHECK_CALLED_ON_VALID_THREAD(my_thread_checker_);
//           ... (do stuff) ...
//       }
//
//    private:
//       THREAD_CHECKER(my_thread_checker_);
//   }

namespace base {

class ThreadChecker
{
public:
    ThreadChecker();
    ~ThreadChecker() = default;

    bool calledOnValidThread() const;
    void detachFromThread();

private:
    void ensureAssigned();

    std::thread::id thread_id_;
    mutable std::mutex thread_id_lock_;

    DISALLOW_COPY_AND_ASSIGN(ThreadChecker);
};

} // namespace base

#ifndef NDEBUG
#define THREAD_CHECKER(name) ::base::ThreadChecker name
#define DCHECK_CALLED_ON_VALID_THREAD(name) DCHECK((name).calledOnValidThread())
#define DETACH_FROM_THREAD(name) (name).detachFromThread()
#else // NDEBUG
#define THREAD_CHECKER(name)
#define DCHECK_CALLED_ON_VALID_THREAD(name)
#define DETACH_FROM_THREAD(name)
#endif // NDEBUG

#endif // BASE_THREADING_THREAD_CHECKER_H
