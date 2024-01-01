//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_CLIENT_SYSTEM_INFO_H
#define CLIENT_CLIENT_SYSTEM_INFO_H

#include "client/client.h"
#include "client/system_info_control.h"

namespace client {

class SystemInfoControlProxy;
class SystemInfoWindowProxy;

class ClientSystemInfo
    : public Client,
      public SystemInfoControl
{
public:
    explicit ClientSystemInfo(std::shared_ptr<base::TaskRunner> io_task_runner);
    ~ClientSystemInfo() override;

    void setSystemInfoWindow(std::shared_ptr<SystemInfoWindowProxy> system_info_window_proxy);

    // SystemInfoControl implementation.
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request) override;

protected:
    // Client implementation.
    void onSessionStarted() override;
    void onSessionMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) override;

private:
    std::shared_ptr<SystemInfoControlProxy> system_info_control_proxy_;
    std::shared_ptr<SystemInfoWindowProxy> system_info_window_proxy_;

    DISALLOW_COPY_AND_ASSIGN(ClientSystemInfo);
};

} // namespace client

#endif // CLIENT_CLIENT_SYSTEM_INFO_H
