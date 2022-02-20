//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_SYSTEM_INFO_CONTROL_PROXY_H
#define CLIENT_SYSTEM_INFO_CONTROL_PROXY_H

#include "base/macros_magic.h"
#include "client/system_info_control.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class SystemInfoControlProxy : public std::enable_shared_from_this<SystemInfoControlProxy>
{
public:
    SystemInfoControlProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                           SystemInfoControl* system_info_control);
    ~SystemInfoControlProxy();

    void dettach();

    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);

private:
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    SystemInfoControl* system_info_control_;

    DISALLOW_COPY_AND_ASSIGN(SystemInfoControlProxy);
};

} // namespace client

#endif // CLIENT_SYSTEM_INFO_CONTROL_PROXY_H
