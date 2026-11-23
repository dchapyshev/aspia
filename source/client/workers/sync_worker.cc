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

#include "client/workers/sync_worker.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_credentials.h"
#include "client/sync/sync_crypto.h"
#include "client/sync/sync_local_groups.h"
#include "client/sync/sync_local_hosts.h"
#include "client/sync/sync_profiles.h"
#include "client/sync/sync_router_hosts.h"
#include "client/workers/router_worker.h"
#include "proto/router.h"
#include "proto/router_constants.h"

namespace {

constexpr Minutes kRecordExchangeInterval{ 10 };
constexpr Seconds kRouterReadingInterval{ 15 };

volatile auto g_eventType = qRegisterMetaType<SyncWorker::Event>();

} // namespace

//--------------------------------------------------------------------------------------------------
SyncWorker::SyncWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SyncWorker::~SyncWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onSessionStarted()
{
    LOG(INFO) << "Session started";

    state_ = State::IDLE;
    reloadConfig();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onSessionFinished()
{
    LOG(INFO) << "Session finished";

    forgetStorage();
    config_.reset();
    state_ = State::OFFLINE;
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onSyncEnabledChanged()
{
    if (state_ == State::OFFLINE)
        return;

    LOG(INFO) << "The synchronization is turned on or off";
    reloadConfig();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onStartRecordExchange()
{
    startRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onStorageResetPending()
{
    if (state_ == State::OFFLINE)
    {
        LOG(INFO) << "The reset of the storage is put off till the session";
        return;
    }

    if (config_.has_value() && config_->isSyncStorageResetPending() &&
        (state_ == State::PREPARING_STORAGE_RESET || state_ == State::SENDING_STORAGE_KEY))
    {
        LOG(INFO) << "The storage is being reset already, state:" << state_;
        return;
    }

    LOG(INFO) << "The storage is to be reset";
    reloadConfig();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onMessageReceived(const QByteArray& buffer)
{
    proto::router::RouterToSync message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse sync message";
        return;
    }

    if (message.has_storage_notification())
        readStorageNotification();
    else if (message.has_storage_info())
        readStorageInfo(message.storage_info());
    else if (message.has_record_list())
        readRecordList(message.record_list());
    else if (message.has_record_result())
        readRecordResult(message.record_result());
    else
        LOG(WARNING) << "Unhandled sync message";
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onPrepare()
{
    RouterWorker* router_worker = findWorker<RouterWorker>();
    if (!router_worker)
    {
        LOG(ERROR) << "Router worker not found";
        return;
    }

    connect(router_worker, &RouterWorker::sig_syncMessageReceived, this, &SyncWorker::onMessageReceived,
            Qt::QueuedConnection);
    connect(this, &SyncWorker::sig_sendMessage, router_worker, &RouterWorker::onSendMessage,
            Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onStop()
{
    onSessionFinished();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::onTimer(TimePoint now)
{
    if (next_exchange_time_ == TimePoint() || now < next_exchange_time_)
        return;

    if (state_ != State::IDLE)
        return;

    LOG(INFO) << "The interval of the exchange has passed";

    if (!config_.has_value())
        reloadConfig();
    else if (storage_id_.isEmpty())
        sendStorageInfoRequest();
    else
        startRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::reloadConfig()
{
    RouterConfig config;
    const Database::FindResult found = RouterStore().find(&config);
    if (found != Database::FindResult::FOUND && found != Database::FindResult::NOT_FOUND)
    {
        LOG(ERROR) << "Unable to read the router";

        forgetStorage();
        config_.reset();
        scheduleRouterReading();
        return;
    }

    if (found == Database::FindResult::NOT_FOUND || !config.isSyncEnabled())
    {
        LOG(INFO) << "No router with the synchronization turned on";

        forgetStorage();
        config_.reset();
        next_exchange_time_ = TimePoint();
        return;
    }

    config_ = config;
    sendStorageInfoRequest();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::startRecordExchange()
{
    if (state_ == State::OFFLINE)
    {
        LOG(INFO) << "No session to exchange in";
        return;
    }

    if (state_ != State::IDLE)
    {
        LOG(INFO) << "The exchange is put off, state:" << state_;
        repeat_requested_ = true;
        return;
    }

    if (storage_id_.isEmpty())
    {
        LOG(INFO) << "No storage to exchange with";
        return;
    }

    startReceivingRecords();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::startReceivingRecords()
{
    LOG(INFO) << "Exchange started";

    Database& db = Database::instance();

    records_.clear();
    records_.emplace_back(std::make_unique<SyncCredentials>(db, storage_id_, key_));
    records_.emplace_back(std::make_unique<SyncProfiles>(db, storage_id_, key_));
    records_.emplace_back(std::make_unique<SyncRouterHosts>(db, storage_id_, key_));
    records_.emplace_back(std::make_unique<SyncLocalGroups>(db, storage_id_, key_));
    records_.emplace_back(std::make_unique<SyncLocalHosts>(db, storage_id_, key_));
    repeat_requested_ = false;

    exchangeRecords(0);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::exchangeRecords(size_t index)
{
    if (index >= records_.size())
    {
        finishRecordExchange();
        return;
    }

    records_index_ = index;
    sendRecordListRequest();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::finishRecordExchange()
{
    LOG(INFO) << "Exchange finished";

    const bool repeat = repeat_requested_;

    state_ = State::IDLE;
    records_.clear();
    scheduleRecordExchange();

    if (repeat)
        startReceivingRecords();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::abortRecordExchange()
{
    if (state_ == State::RECEIVING_RECORDS || state_ == State::SENDING_RECORDS)
        LOG(WARNING) << "Exchange aborted, state:" << state_;

    state_ = State::IDLE;
    records_.clear();
    repeat_requested_ = false;
    scheduleRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::announceFailure(SyncRecords::SyncResult result)
{
    if (result == SyncRecords::SyncResult::DETACHED)
    {
        LOG(INFO) << "The device has left the synchronization meanwhile";
    }
    else if (result == SyncRecords::SyncResult::INVALID_ANSWER)
    {
        LOG(ERROR) << "The router returned an invalid answer";
        emit sig_event(Event::INVALID_ROUTER_ANSWER, QString());
    }
    else if (result == SyncRecords::SyncResult::FAILED)
    {
        LOG(ERROR) << "The device failed to keep the synchronization data";
        emit sig_event(Event::DEVICE_ERROR, QString());
    }
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::scheduleRecordExchange()
{
    next_exchange_time_ = Clock::now() + kRecordExchangeInterval;
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::scheduleRouterReading()
{
    next_exchange_time_ = Clock::now() + kRouterReadingInterval;
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::attachStorage(bool reset)
{
    const bool another_storage = config_->syncStorageId() != storage_id_;

    const Database::SyncResult result = RouterStore().setSyncStorage(*config_, storage_id_, key_, reset);
    if (result == Database::SyncResult::DETACHED)
    {
        LOG(INFO) << "The device has left the synchronization meanwhile";
        forgetStorage();
        return;
    }

    if (result != Database::SyncResult::DONE)
    {
        LOG(ERROR) << "Unable to attach to the storage" << storage_id_.toHex();
        emit sig_event(Event::DEVICE_ERROR, QString());
        forgetStorage();
        return;
    }

    LOG(INFO) << "Attached to the storage" << storage_id_.toHex() << "another:" << another_storage;

    config_->setSyncStorageId(storage_id_);
    config_->setSyncStorageResetPending(false);

    state_ = State::IDLE;
    emit sig_event(Event::STORAGE_ATTACHED, QString());

    if (another_storage)
    {
        emit sig_recordsChanged(proto::router::SYNC_DATA_TYPE_CREDENTIALS);
        emit sig_recordsChanged(proto::router::SYNC_DATA_TYPE_PROFILES);
        emit sig_recordsChanged(proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS);
        emit sig_recordsChanged(proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS);
    }

    startRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::forgetStorage()
{
    abortRecordExchange();
    storage_id_.clear();
    key_.clear();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::sendStorageInfoRequest()
{
    forgetStorage();
    state_ = config_->isSyncStorageResetPending() ? State::PREPARING_STORAGE_RESET : State::REQUESTING_STORAGE_INFO;

    LOG(INFO) << "Asking for the storage, state:" << state_;

    proto::router::SyncToRouter message;
    message.mutable_storage_info_request()->set_request_id(++request_id_);

    send(message);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::sendStorageKeyRequest(proto::router::SyncStorageKeyRequest::Mode mode, qint64 key_revision)
{
    proto::router::SyncToRouter message;

    proto::router::SyncStorageKeyRequest* request = message.mutable_storage_key_request();
    if (!SyncCrypto::wrapStorageKey(key_, config_->password(), request->mutable_key()))
    {
        LOG(ERROR) << "Unable to wrap the key of the storage";
        emit sig_event(Event::DEVICE_ERROR, QString());
        forgetStorage();
        return;
    }

    request->set_request_id(++request_id_);
    request->set_mode(mode);
    request->set_storage_id(storage_id_.toStdString());
    request->set_key_revision(key_revision);

    state_ = State::SENDING_STORAGE_KEY;

    send(message);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::sendRecordListRequest()
{
    state_ = State::RECEIVING_RECORDS;

    proto::router::SyncToRouter message;

    proto::router::SyncRecordListRequest* request = message.mutable_record_list_request();
    request->set_request_id(++request_id_);
    request->set_storage_id(storage_id_.toStdString());
    request->set_type(records_[records_index_]->type());
    request->set_revision(records_[records_index_]->receivedRevision());
    request->set_count(proto::router::kMaxSyncPageSize);

    LOG(INFO) << "Asking for the records of type" << request->type() << "newer than revision" << request->revision();

    send(message);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::sendRecordRequest()
{
    state_ = State::SENDING_RECORDS;

    proto::router::SyncToRouter message;

    proto::router::SyncRecordRequest* request = message.mutable_record_request();
    const SyncRecords::SyncResult read = records_[records_index_]->readUnsyncedRecords(request);
    if (read != SyncRecords::SyncResult::DONE)
    {
        LOG(INFO) << "The records to send are not read";
        announceFailure(read);
        abortRecordExchange();
        return;
    }

    if (request->record().empty())
    {
        exchangeRecords(records_index_ + 1);
        return;
    }

    request->set_request_id(++request_id_);
    request->set_storage_id(storage_id_.toStdString());

    LOG(INFO) << "Sending records:" << request->record_size();

    send(message);
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::readStorageNotification()
{
    LOG(INFO) << "The router tells of a change, state:" << state_;

    if (state_ == State::IDLE && storage_id_.isEmpty() && config_.has_value())
        sendStorageInfoRequest();
    else
        startRecordExchange();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::readStorageInfo(const proto::router::SyncStorageInfo& info)
{
    if (info.request_id() != request_id_)
    {
        LOG(WARNING) << "Answer about the storage to request" << info.request_id() << "is not awaited";
        return;
    }

    const std::string& error_code = info.error_code();

    if (error_code == proto::router::kErrorOk &&
        info.storage_id().size() != static_cast<size_t>(proto::router::kSyncStorageIdSize))
    {
        LOG(ERROR) << "Wrong size of the storage id:" << info.storage_id().size();
        emit sig_event(Event::INVALID_ROUTER_ANSWER, QString());
        forgetStorage();
        return;
    }

    if (state_ == State::REQUESTING_STORAGE_INFO || state_ == State::PREPARING_STORAGE_RESET)
    {
        if (error_code == proto::router::kErrorOk)
        {
            if (state_ == State::PREPARING_STORAGE_RESET)
            {
                storage_id_ = QByteArray::fromStdString(info.storage_id());
                key_ = SyncCrypto::generateStorageKey();

                LOG(INFO) << "Resetting the storage" << storage_id_.toHex();
                sendStorageKeyRequest(proto::router::SyncStorageKeyRequest::MODE_RESET, 0);
            }
            else
            {
                readStorageKey(info);
            }
        }
        else if (error_code == proto::router::kErrorNotFound)
        {
            key_ = SyncCrypto::generateStorageKey();

            const proto::router::SyncStorageKeyRequest::Mode mode = config_->isSyncStorageResetPending() ?
                proto::router::SyncStorageKeyRequest::MODE_RESET : proto::router::SyncStorageKeyRequest::MODE_CREATE;

            LOG(INFO) << "The router holds no storage, creating one, mode:" << mode;
            sendStorageKeyRequest(mode, 0);
        }
        else
        {
            LOG(ERROR) << "The router did not return the storage:" << error_code;
            emit sig_event(Event::ROUTER_ERROR, QString::fromStdString(error_code));
            forgetStorage();
        }
    }
    else if (state_ == State::SENDING_STORAGE_KEY)
    {
        if (error_code == proto::router::kErrorOk)
        {
            storage_id_ = QByteArray::fromStdString(info.storage_id());
            attachStorage(info.reset());
        }
        else if (error_code == proto::router::kErrorConflict)
        {
            LOG(INFO) << "The key of the storage is already wrapped anew";
            attachStorage(info.reset());
        }
        else if (error_code == proto::router::kErrorAlreadyExists || error_code == proto::router::kErrorNotFound)
        {
            LOG(INFO) << "The storage has changed meanwhile:" << error_code;
            sendStorageInfoRequest();
        }
        else
        {
            LOG(ERROR) << "The router did not store the key of the storage:" << error_code;
            emit sig_event(Event::ROUTER_ERROR, QString::fromStdString(error_code));
            forgetStorage();
        }
    }
    else
    {
        LOG(WARNING) << "Answer about the storage is not awaited, state:" << state_;
    }
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::readStorageKey(const proto::router::SyncStorageInfo& info)
{
    const QByteArray storage_id = QByteArray::fromStdString(info.storage_id());
    SecureByteArray key;

    const SyncCrypto::Result result = SyncCrypto::unwrapStorageKey(info.key(), config_->password(), &key);

    RouterConfig stored;
    const Database::FindResult found = RouterStore().find(&stored);
    if (found != Database::FindResult::FOUND && found != Database::FindResult::NOT_FOUND)
    {
        LOG(ERROR) << "Unable to read the router";

        forgetStorage();
        scheduleRouterReading();
        return;
    }

    const bool same_storage =
        found == Database::FindResult::FOUND &&
        stored.syncStorageId() == storage_id &&
        !stored.syncStorageKey().isEmpty();

    if (result == SyncCrypto::Result::SUCCESS)
    {
        if (same_storage && stored.syncStorageKey() != key)
        {
            LOG(ERROR) << "The router returned another key for the storage" << storage_id.toHex();
            emit sig_event(Event::INVALID_ROUTER_ANSWER, QString());
            forgetStorage();
            return;
        }

        storage_id_ = storage_id;
        key_ = key;

        attachStorage(info.reset());
    }
    else if (result == SyncCrypto::Result::WRONG_PASSWORD)
    {
        if (!same_storage)
        {
            LOG(INFO) << "The key of the storage does not open with the password of the account";
            emit sig_event(Event::STORAGE_KEY_NOT_OPENED, QString());
            forgetStorage();
            next_exchange_time_ = TimePoint();
            return;
        }

        storage_id_ = storage_id;
        key_ = stored.syncStorageKey();

        LOG(INFO) << "Wrapping the key of the storage anew with the password of the account";
        sendStorageKeyRequest(proto::router::SyncStorageKeyRequest::MODE_MODIFY, info.key_revision());
    }
    else
    {
        LOG(ERROR) << "The router returned a key that cannot be read";
        emit sig_event(Event::INVALID_ROUTER_ANSWER, QString());
        forgetStorage();
    }
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::readRecordList(const proto::router::SyncRecordList& list)
{
    if (state_ != State::RECEIVING_RECORDS || list.request_id() != request_id_)
    {
        LOG(WARNING) << "List of records to request" << list.request_id() << "is not awaited, state:" << state_;
        return;
    }

    if (list.error_code() == proto::router::kErrorNotFound)
    {
        LOG(WARNING) << "The router does not hold the storage any more";
        sendStorageInfoRequest();
        return;
    }

    if (list.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "The router did not return the records:" << list.error_code();
        emit sig_event(Event::ROUTER_ERROR, QString::fromStdString(list.error_code()));
        abortRecordExchange();
        return;
    }

    SyncRecords& records = *records_[records_index_];
    const qint64 revision = records.receivedRevision();

    const SyncRecords::SyncResult stored = records.storeReceivedRecords(list);
    if (stored != SyncRecords::SyncResult::DONE)
    {
        LOG(INFO) << "The received records are not stored";
        announceFailure(stored);
        abortRecordExchange();
        return;
    }

    LOG(INFO) << "Records received:" << list.record_size();

    if (!list.record().empty())
        emit sig_recordsChanged(records.type());

    if (!list.has_more())
    {
        sendRecordRequest();
        return;
    }

    if (records.receivedRevision() <= revision)
    {
        LOG(ERROR) << "The router returned a page that does not move the reading on";
        emit sig_event(Event::INVALID_ROUTER_ANSWER, QString());
        abortRecordExchange();
        return;
    }

    sendRecordListRequest();
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::readRecordResult(const proto::router::SyncRecordResult& result)
{
    if (state_ != State::SENDING_RECORDS || result.request_id() != request_id_)
    {
        LOG(WARNING) << "Result of records to request" << result.request_id() << "is not awaited, state:"
                     << state_;
        return;
    }

    if (result.error_code() == proto::router::kErrorNotFound)
    {
        LOG(WARNING) << "The router does not hold the storage any more";
        sendStorageInfoRequest();
        return;
    }

    if (result.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "The router did not store the records:" << result.error_code();
        emit sig_event(Event::ROUTER_ERROR, QString::fromStdString(result.error_code()));

        const SyncRecords::SyncResult discarded = records_[records_index_]->discardSentRecords();
        if (discarded != SyncRecords::SyncResult::DONE)
            announceFailure(discarded);

        abortRecordExchange();
        return;
    }

    const SyncRecords::SyncResult confirmed = records_[records_index_]->confirmSentRecords(result);
    if (confirmed != SyncRecords::SyncResult::DONE)
    {
        LOG(INFO) << "The sent records are not confirmed";
        announceFailure(confirmed);
        abortRecordExchange();
        return;
    }

    bool record_refused = false;

    for (qint64 revision : result.record_revision())
    {
        if (revision == 0)
            record_refused = true;
    }

    if (record_refused)
    {
        LOG(INFO) << "The router refused a record, it waits for the next exchange";
        exchangeRecords(records_index_ + 1);
    }
    else
    {
        sendRecordRequest();
    }
}

//--------------------------------------------------------------------------------------------------
void SyncWorker::send(const proto::router::SyncToRouter& message)
{
    emit sig_sendMessage(proto::router::CHANNEL_ID_SYNC, serialize(message));
}
