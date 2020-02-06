//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__USER_SESSION_WINDOW_H
#define HOST__USER_SESSION_WINDOW_H

#include "host/user_session_agent.h"

namespace host {

class UserSessionWindow
{
public:
    virtual ~UserSessionWindow() = default;

    virtual void onStateChanged(UserSessionAgent::State state) = 0;
    virtual void onClientListChanged(const UserSessionAgent::ClientList& clients) = 0;
    virtual void onCredentialsChanged(const proto::internal::Credentials& credentials) = 0;
};

} // namespace host

#endif // HOST__USER_SESSION_WINDOW_H
