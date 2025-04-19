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

#ifndef CLIENT_PORT_FORWARDING_WINDOW_PROXY_H
#define CLIENT_PORT_FORWARDING_WINDOW_PROXY_H

#include "base/macros_magic.h"
#include "client/port_forwarding_window.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class PortForwardingWindowProxy final
    : public std::enable_shared_from_this<PortForwardingWindowProxy>
{
public:
    PortForwardingWindowProxy(std::shared_ptr<base::TaskRunner> ui_task_runner,
                              PortForwardingWindow* port_forwarding_window);
    ~PortForwardingWindowProxy();

    void dettach();

    void start();
    void setStatistics(const PortForwardingWindow::Statistics& statistics);

private:
    std::shared_ptr<base::TaskRunner> ui_task_runner_;
    PortForwardingWindow* port_forwarding_window_;

    DISALLOW_COPY_AND_ASSIGN(PortForwardingWindowProxy);
};

} // namespace client

#endif // CLIENT_PORT_FORWARDING_WINDOW_PROXY_H
