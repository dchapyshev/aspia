//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__CONNECT_DATA_H
#define ASPIA_CLIENT__CONNECT_DATA_H

#include <QString>

#include "build/build_config.h"
#include "proto/common.pb.h"
#include "proto/desktop_session.pb.h"

namespace client {

struct ConnectData
{
    ConnectData();
    ~ConnectData();

    QString computer_name;
    QString address;
    uint16_t port = DEFAULT_HOST_TCP_PORT;
    QString username;
    QString password;

    proto::SessionType session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
    proto::desktop::Config desktop_config;
};

} // namespace client

#endif // ASPIA_CLIENT__CONNECT_DATA_H
