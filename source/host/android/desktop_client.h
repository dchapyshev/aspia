//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_ANDROID_DESKTOP_CLIENT_H
#define HOST_ANDROID_DESKTOP_CLIENT_H

#include "host/client.h"

class DesktopClient final : public Client
{
    Q_OBJECT

public:
    explicit DesktopClient(TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~DesktopClient() final;

protected:
    // Client implementation.
    void onStart() final;
    void onMessage(quint8 channel_id, const QByteArray& buffer) final;

private:
    Q_DISABLE_COPY_MOVE(DesktopClient)
};

#endif // HOST_ANDROID_DESKTOP_CLIENT_H
