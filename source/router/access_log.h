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

#ifndef ROUTER_ACCESS_LOG_H
#define ROUTER_ACCESS_LOG_H

#include "base/peer/host_id.h"
#include "base/sql/sql_database.h"

namespace proto::router {
class AccessLogFilter;
class AccessLogList;
} // namespace proto::router

class AccessLog
{
public:
    struct Record
    {
        qint64 user_id = 0;
        std::string user_name;
        quint32 session_type = 0;
        std::string client_address;
        std::string client_computer_name;
        std::string client_os_name;
        std::string client_version;
        HostId host_id = kInvalidHostId;
        std::string host_computer_name;
        std::string host_address;
    };

    AccessLog() = default;
    ~AccessLog() = default;

    static QString filePath();

    bool open(const QString& file_path);
    void close();
    bool isValid() const;
    bool addEntry(const Record& record);

    // The newest entries first. Only those up to |max_entry_id| are taken, and 0 takes them up to the
    // newest one.
    void entries(const proto::router::AccessLogFilter& filter, qint64 max_entry_id, qint64 offset,
        qint64 count, proto::router::AccessLogList* out) const;

    bool prune(int retention_days);

private:
    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(AccessLog)
};

#endif // ROUTER_ACCESS_LOG_H
