//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__CLIENT_SESSION_DESKTOP_H
#define HOST__CLIENT_SESSION_DESKTOP_H

#include "base/macros_magic.h"
#include "host/client_session.h"

namespace host {

class ClientSessionDesktop : public ClientSession
{
public:
    ClientSessionDesktop(proto::SessionType session_type,
                         const QString& username,
                         std::unique_ptr<net::Channel> channel);
    ~ClientSessionDesktop();

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSessionDesktop);
};

} // namespace host

#endif // HOST__CLIENT_SESSION_DESKTOP_H
