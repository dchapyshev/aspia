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

#ifndef ROUTER_SYNC_STORAGE_H
#define ROUTER_SYNC_STORAGE_H

#include <QStringList>

#include "base/sql/sql_database.h"

namespace proto::router {
class SyncRecordList;
class SyncRecordListRequest;
class SyncRecordRequest;
class SyncRecordResult;
class SyncStorageInfo;
class SyncStorageKeyRequest;
} // namespace proto::router

class SyncStorage final
{
public:
    SyncStorage() = default;
    ~SyncStorage() = default;

    static QStringList userGuids();
    static bool remove(const QString& user_guid);

    static void setDirectoryPathForTesting(const QString& directory_path);

    bool open(const QString& user_guid);
    void close();
    bool isValid() const;

    void readStorageInfo(proto::router::SyncStorageInfo* out) const;
    void createStorage(const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out);
    void changeKey(const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out);
    void resetStorage(const proto::router::SyncStorageKeyRequest& request, proto::router::SyncStorageInfo* out);

    void readRecords(const proto::router::SyncRecordListRequest& request,
        proto::router::SyncRecordList* out) const;
    void storeRecords(const proto::router::SyncRecordRequest& request,
        proto::router::SyncRecordResult* out, bool* records_changed);

private:
    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(SyncStorage)
};

#endif // ROUTER_SYNC_STORAGE_H
