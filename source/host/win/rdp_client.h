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

#ifndef HOST_WIN_RDP_CLIENT_H
#define HOST_WIN_RDP_CLIENT_H

#include "base/macros_magic.h"
#include "base/desktop/geometry.h"
#include "base/threading/thread_checker.h"

#include <cstdint>
#include <memory>
#include <string>

namespace base {
class TaskRunner;
} // namespace base

namespace host {

// Establishes a loopback RDP connection to spawn a new Windows session.
class RdpClient
{
public:
    class EventHandler
    {
    public:
        virtual ~EventHandler() = default;

        // Notifies the event handler that an RDP connection has been established successfully.
        virtual void onRdpConnected() = 0;

        // Notifies that the RDP connection has been closed.
        virtual void onRdpClosed() = 0;
    };

    RdpClient(std::shared_ptr<base::TaskRunner> caller_task_runner,
              std::shared_ptr<base::TaskRunner> ui_task_runner,
              const base::Size& resolution,
              const std::string& terminal_id,
              uint32_t port_number,
              EventHandler* event_handler);

    virtual ~RdpClient();

    // Sends Secure Attention Sequence to the session.
    void injectSas();

    // Change the resolution of the desktop.
    void changeResolution(const base::Size& resolution);

private:
    // The actual implementation resides in Core class.
    class Core;
    std::shared_ptr<Core> core_;

    THREAD_CHECKER(sequence_checker_);
    DISALLOW_COPY_AND_ASSIGN(RdpClient);
};

} // namespace host

#endif // HOST_WIN_RDP_CLIENT_H
