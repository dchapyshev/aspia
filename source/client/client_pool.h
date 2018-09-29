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

#ifndef ASPIA_CLIENT__CLIENT_POOL_H_
#define ASPIA_CLIENT__CLIENT_POOL_H_

#include "base/macros_magic.h"
#include "client/connect_data.h"

namespace aspia {

class Client;

class ClientPool
{
public:
    static void connect(const ConnectData& connect_data);
    static void connect();

private:
    DISALLOW_COPY_AND_ASSIGN(ClientPool);
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_POOL_H_
