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

#ifndef CLIENT_CLIENT_SYSTEM_INFO_H
#define CLIENT_CLIENT_SYSTEM_INFO_H

#include "client/client.h"
#include "proto/system_info.h"

namespace client {

class ClientSystemInfo final : public Client
{
    Q_OBJECT

public:
    explicit ClientSystemInfo(QObject* parent = nullptr);
    ~ClientSystemInfo() final;

public slots:
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);

signals:
    void sig_systemInfo(const proto::system_info::SystemInfo& system_info);

protected:
    // Client implementation.
    void onSessionStarted() final;
    void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(uint8_t channel_id, size_t pending) final;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSystemInfo);
};

} // namespace client

#endif // CLIENT_CLIENT_SYSTEM_INFO_H
