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

#ifndef ASPIA_CLIENT__CLIENT_POOL_H
#define ASPIA_CLIENT__CLIENT_POOL_H

#include <optional>

#include "base/macros_magic.h"
#include "client/connect_data.h"

namespace aspia {

class Client;

class ClientPool
{
public:
    // Connects to the computer.
    // If you do not specify the connection parameters, then displays a dialog for setting the
    // parameters.
    // If the connection parameters do not specify a user name or a password, it displays a dialog
    // for authorization.
    // If the connection is canceled by the user, it returns |false|, otherwise |true|.
    static bool connect(const std::optional<ConnectData>& connect_data = std::nullopt);

private:
    DISALLOW_COPY_AND_ASSIGN(ClientPool);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_POOL_H
