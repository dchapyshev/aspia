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

#ifndef ASPIA_CLIENT__CONNECT_DATA_H_
#define ASPIA_CLIENT__CONNECT_DATA_H_

#include "protocol/desktop_session.pb.h"
#include "protocol/session_type.pb.h"

namespace aspia {

struct ConnectData
{
    ~ConnectData();

    std::string computer_name;
    std::string address;
    uint16_t port = 0;
    std::string username;
    std::string password;

    proto::SessionType session_type = proto::SESSION_TYPE_UNKNOWN;
    proto::desktop::Config desktop_config;
};

} // namespace aspia

#endif // ASPIA_CLIENT__CONNECT_DATA_H_
