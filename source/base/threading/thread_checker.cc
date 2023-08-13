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

#include "base/threading/thread_checker.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ThreadChecker::ThreadChecker()
{
    ensureAssigned();
}

//--------------------------------------------------------------------------------------------------
bool ThreadChecker::calledOnValidThread() const
{
    std::scoped_lock lock(thread_id_lock_);
    return thread_id_ == std::this_thread::get_id();
}

//--------------------------------------------------------------------------------------------------
void ThreadChecker::detachFromThread()
{
    std::scoped_lock lock(thread_id_lock_);
    thread_id_ = std::thread::id();
}

//--------------------------------------------------------------------------------------------------
void ThreadChecker::ensureAssigned()
{
    std::scoped_lock lock(thread_id_lock_);
    thread_id_ = std::this_thread::get_id();
}

} // namespace base
