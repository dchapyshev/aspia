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

#ifndef CLIENT_WORKERS_SYNC_WORKER_H
#define CLIENT_WORKERS_SYNC_WORKER_H

#include <optional>

#include "base/threading/worker.h"
#include "client/config.h"
#include "client/sync/sync_records.h"

class SyncWorker final : public Worker
{
    Q_OBJECT

public:
    SyncWorker();
    ~SyncWorker() final;

    enum class Event
    {
        STORAGE_ATTACHED,
        STORAGE_KEY_NOT_OPENED,
        ROUTER_ERROR,
        INVALID_ROUTER_ANSWER,
        DEVICE_ERROR
    };
    Q_ENUM(Event)

    enum class State
    {
        OFFLINE,
        IDLE,
        REQUESTING_STORAGE_INFO,
        PREPARING_STORAGE_RESET,
        SENDING_STORAGE_KEY,
        RECEIVING_RECORDS,
        SENDING_RECORDS
    };
    Q_ENUM(State)

public slots:
    void onSessionStarted();
    void onSessionFinished();
    void onSyncEnabledChanged();
    void onStartRecordExchange();
    void onStorageResetPending();
    void onMessageReceived(const QByteArray& buffer);

signals:
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);
    void sig_recordsChanged(proto::router::SyncDataType type);
    void sig_event(SyncWorker::Event event, const QString& error_code);

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private:
    friend class SyncWorkerTestPeer;

    void reloadConfig();
    void startRecordExchange();
    void startReceivingRecords();
    void exchangeRecords(size_t index);
    void finishRecordExchange();
    void abortRecordExchange();
    void announceFailure(SyncRecords::SyncResult result);
    void scheduleRecordExchange();
    void scheduleRouterReading();
    void attachStorage(bool reset);
    void forgetStorage();
    void sendStorageInfoRequest();
    void sendStorageKeyRequest(proto::router::SyncStorageKeyRequest::Mode mode, qint64 key_revision);
    void sendRecordListRequest();
    void sendRecordRequest();
    void readStorageNotification();
    void readStorageInfo(const proto::router::SyncStorageInfo& info);
    void readStorageKey(const proto::router::SyncStorageInfo& info);
    void readRecordList(const proto::router::SyncRecordList& list);
    void readRecordResult(const proto::router::SyncRecordResult& result);
    void send(const proto::router::SyncToRouter& message);

    State state_ = State::OFFLINE;
    std::optional<RouterConfig> config_;
    std::vector<std::unique_ptr<SyncRecords>> records_;
    size_t records_index_ = 0;
    QByteArray storage_id_;
    SecureByteArray key_;
    qint64 request_id_ = 0;
    bool repeat_requested_ = false;
    TimePoint next_exchange_time_;

    Q_DISABLE_COPY_MOVE(SyncWorker)
};

Q_DECLARE_METATYPE(SyncWorker::Event)

#endif // CLIENT_WORKERS_SYNC_WORKER_H
