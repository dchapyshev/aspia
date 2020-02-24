//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/task_runner.h"

namespace base {

namespace {

class DeleteHelper
{
public:
    DeleteHelper(void(*deleter)(const void*), const void* object)
        : deleter_(deleter),
          object_(object)
    {
        // Nothing
    }

    ~DeleteHelper()
    {
        doDelete();
    }

    void doDelete()
    {
        if (deleter_ && object_)
        {
            deleter_(object_);

            deleter_ = nullptr;
            object_ = nullptr;
        }
    }

private:
    void(*deleter_)(const void*);
    const void* object_;

    DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

} // namespace

void TaskRunner::deleteSoonInternal(void(*deleter)(const void*), const void* object)
{
    postNonNestableTask(
        std::bind(&DeleteHelper::doDelete, std::make_shared<DeleteHelper>(deleter, object)));
}

} // namespace base
