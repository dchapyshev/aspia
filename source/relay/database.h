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

#ifndef RELAY__DATABASE_H
#define RELAY__DATABASE_H

#include "base/peer/host_id.h"

#include <cstdint>
#include <ctime>
#include <vector>

namespace relay {

class Database
{
public:
    virtual ~Database() = default;

    struct Session
    {
        int64_t entry_id;
        int64_t client_id;
        base::HostId host_id;
        time_t start_time;
        time_t end_time;
        int64_t bytes_rx;
        int64_t bytes_tx;
    };

    using SessionList = std::vector<Session>;

    virtual bool addSession(const Session& session) = 0;
    virtual bool removeSession(int64_t entry_id) = 0;
    virtual SessionList sessions(int64_t limit) = 0;
};

} // namespace relay

#endif // RELAY__DATABASE_H
