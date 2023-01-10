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

#ifndef COMMON_UPDATE_CHECKER_H
#define COMMON_UPDATE_CHECKER_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/memory/byte_array.h"
#include "base/threading/simple_thread.h"

namespace common {

class UpdateChecker
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void onUpdateCheckedFinished(const base::ByteArray& result) = 0;
    };

    UpdateChecker();
    ~UpdateChecker();

    void setUpdateServer(std::string_view update_server);
    void setPackageName(std::string_view package_name);

    void start(std::shared_ptr<base::TaskRunner> owner_task_runner, Delegate* delegate);

private:
    void run();
    void onFinished(const base::ByteArray& response);

    std::shared_ptr<base::TaskRunner> owner_task_runner_;
    Delegate* delegate_ = nullptr;
    base::SimpleThread thread_;
    std::string update_server_;
    std::string package_name_;

    DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

} // namespace common

#endif // COMMON_UPDATE_CHECKER_H
