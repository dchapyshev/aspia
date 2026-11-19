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

#ifndef CLIENT_SYNC_SYNC_CREDENTIALS_H
#define CLIENT_SYNC_SYNC_CREDENTIALS_H

#include <QList>
#include <QString>

#include "base/crypto/secure_byte_array.h"
#include "proto/router_sync.h"

class Database;

class SyncCredentials
{
public:
    SyncCredentials(Database& db, const QByteArray& storage_id, const SecureByteArray& key);

    enum class Result { DONE, DETACHED, INVALID_ANSWER, FAILED };

    Result readUnsyncedRecords(proto::router::SyncRecordRequest* request);
    Result confirmSentRecords(const proto::router::SyncRecordResult& result);
    Result storeReceivedRecords(const proto::router::SyncRecordList& list);

private:
    struct SentRecord
    {
        QString guid;
        qint64 sync_dirty = 0;
    };

    Database& db_;
    const QByteArray storage_id_;
    const SecureByteArray key_;
    QList<SentRecord> sent_records_;
    qint64 storage_revision_ = -1;

    Q_DISABLE_COPY_MOVE(SyncCredentials)
};

#endif // CLIENT_SYNC_SYNC_CREDENTIALS_H
