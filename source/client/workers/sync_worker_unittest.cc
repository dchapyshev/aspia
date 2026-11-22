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

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/net/udp_channel.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/router_host_store.h"
#include "client/storage/router_store.h"
#include "client/sync/sync_credentials.h"
#include "client/sync/sync_crypto.h"
#include "client/sync/sync_profiles.h"
#include "proto/router.h"

// Test-only access to the timer of the worker, the time is handed in.
class SyncWorkerTestPeer
{
public:
    static void runTimer(SyncWorker& worker, TimePoint now)
    {
        worker.onTimer(now);
    }
};

namespace {

const qint64 kRouterTime = 1000000;
const char kStorageId[] = "0123456789abcdef";
const char kAnotherStorageId[] = "fedcba9876543210";
const char kPassword[] = "secret";
const char kNewPassword[] = "new-secret";

// The key of a storage and what the router holds of it: the same key wrapped with a password of
// the account.
struct StorageKey
{
    SecureByteArray key;
    proto::router::SyncStorageKey wrapped;
};

StorageKey makeStorageKey(const char* password)
{
    StorageKey storage_key;
    storage_key.key = SyncCrypto::generateStorageKey();

    EXPECT_TRUE(SyncCrypto::wrapStorageKey(storage_key.key, SecureString(QString(password)), &storage_key.wrapped));
    return storage_key;
}

// Wrapping a key takes a noticeable time, so the storage of every test starts with the same key.
const StorageKey& storageKey()
{
    static const StorageKey storage_key = makeStorageKey(kPassword);
    return storage_key;
}

// A device with its worker, the router and another device of the same user. The worker belongs to
// its own thread that never starts here, so its slots are called and its signals are taken directly
// in the test thread, and the database of the device is the one of the test thread. The router is
// played by the fixture: it holds one storage with its wrapped key, keeps the records of the
// storage, stores a record only over the revision the change was based on, and tells the device
// that the records have changed. The other device has no worker: it exchanges its records with the
// router when a test tells it to.
class SyncWorkerTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        DatabaseTestPeer::setFilePath(dir_.filePath("laptop.db3"));
        ASSERT_TRUE(Database::instance().isValid());

        RouterStore laptop_store;
        ASSERT_TRUE(laptop_store.save(config()));
        ASSERT_EQ(laptop_store.setSyncStorage(config(), kStorageId, key_, false), Database::SyncResult::DONE);

        ASSERT_TRUE(phone_db_.open(dir_.filePath("phone.db3")));

        RouterStore phone_store(phone_db_);
        ASSERT_TRUE(phone_store.save(config()));
        ASSERT_EQ(phone_store.setSyncStorage(config(), kStorageId, key_, false), Database::SyncResult::DONE);

        QObject::connect(&worker_, &SyncWorker::sig_sendMessage, &worker_,
                         [this](quint8 channel_id, const QByteArray& buffer)
        {
            EXPECT_EQ(channel_id, proto::router::CHANNEL_ID_SYNC);
            onWorkerMessage(buffer);
        }, Qt::DirectConnection);

        QObject::connect(&worker_, &SyncWorker::sig_recordsChanged, &worker_,
                         [this](proto::router::SyncDataType type)
        {
            if (type == proto::router::SYNC_DATA_TYPE_PROFILES)
                ++profiles_changed_count_;
            else if (type == proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS)
                ++router_hosts_changed_count_;
            else
                ++credentials_changed_count_;
        }, Qt::DirectConnection);

        QObject::connect(&worker_, &SyncWorker::sig_event, &worker_,
                         [this](SyncWorker::Event event, const QString& error_code)
        {
            events_.append({ event, error_code });
        }, Qt::DirectConnection);
    }

    // The record of the router with the synchronization turned on.
    static RouterConfig config(const char* password = kPassword)
    {
        RouterConfig config;
        config.setAddress("router.example.com");
        config.setUsername("user");
        config.setPassword(SecureString(QString(password)));
        config.setSyncEnabled(true);
        return config;
    }

    static RouterConfig storedRouter()
    {
        RouterConfig router;
        EXPECT_EQ(RouterStore().find(&router), Database::FindResult::FOUND);
        return router;
    }

    void onWorkerMessage(const QByteArray& buffer)
    {
        proto::router::SyncToRouter message;
        ASSERT_TRUE(parse(buffer, &message));

        if (message.has_storage_info_request())
        {
            ++storage_info_request_count_;
        }
        else if (message.has_storage_key_request())
        {
            ++storage_key_request_count_;

            if (before_storage_key_request_)
                std::exchange(before_storage_key_request_, nullptr)();
        }
        else if (message.has_record_list_request())
        {
            list_request_types_.append(message.record_list_request().type());

            if (message.record_list_request().type() == proto::router::SYNC_DATA_TYPE_PROFILES)
            {
                ++profile_list_request_count_;
            }
            else if (message.record_list_request().type() == proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS)
            {
                ++router_host_list_request_count_;
            }
            else
            {
                ++list_request_count_;

                if (before_listing_)
                    std::exchange(before_listing_, nullptr)();
            }
        }
        else if (message.has_record_request())
        {
            const proto::router::SyncRecordRequest& request = message.record_request();

            if (!request.record().empty() && request.record(0).type() == proto::router::SYNC_DATA_TYPE_PROFILES)
            {
                ++profile_record_request_count_;
            }
            else if (!request.record().empty() &&
                     request.record(0).type() == proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS)
            {
                ++router_host_record_request_count_;
            }
            else
            {
                ++record_request_count_;

                if (before_storing_)
                    std::exchange(before_storing_, nullptr)();
            }
        }

        worker_.onMessageReceived(serialize(routerReply(message)));
    }

    proto::router::RouterToSync routerReply(const proto::router::SyncToRouter& message)
    {
        proto::router::RouterToSync reply;

        if (message.has_storage_info_request())
        {
            proto::router::SyncStorageInfo* info = reply.mutable_storage_info();
            info->set_request_id(message.storage_info_request().request_id());

            writeStorageInfo(info);
        }
        else if (message.has_storage_key_request())
        {
            const proto::router::SyncStorageKeyRequest& request = message.storage_key_request();

            proto::router::SyncStorageInfo* info = reply.mutable_storage_info();
            info->set_request_id(request.request_id());

            if (!error_code_.empty())
            {
                info->set_error_code(error_code_);
                return reply;
            }

            if (request.mode() == proto::router::SyncStorageKeyRequest::MODE_CREATE ||
                (request.mode() == proto::router::SyncStorageKeyRequest::MODE_RESET && storage_id_.empty()))
            {
                if (!storage_id_.empty())
                {
                    info->set_error_code(proto::router::kErrorAlreadyExists);
                    return reply;
                }

                storage_id_ = kStorageId;
                storage_reset_ = request.mode() == proto::router::SyncStorageKeyRequest::MODE_RESET;
            }
            else
            {
                if (storage_id_.empty() || request.storage_id() != storage_id_)
                {
                    info->set_error_code(proto::router::kErrorNotFound);
                    return reply;
                }

                if (request.mode() == proto::router::SyncStorageKeyRequest::MODE_RESET)
                {
                    storage_id_ = kAnotherStorageId;
                    storage_reset_ = true;
                    records_.clear();
                    revision_ = 0;
                }
                else
                {
                    EXPECT_EQ(request.mode(), proto::router::SyncStorageKeyRequest::MODE_MODIFY);

                    if (request.key_revision() != key_revision_)
                    {
                        info->set_error_code(proto::router::kErrorConflict);
                        return reply;
                    }
                }
            }

            wrapped_key_.CopyFrom(request.key());
            ++key_revision_;

            writeStorageInfo(info);
        }
        else if (message.has_record_list_request())
        {
            const proto::router::SyncRecordListRequest& request = message.record_list_request();

            proto::router::SyncRecordList* list = reply.mutable_record_list();
            list->set_request_id(request.request_id());
            list->set_error_code(errorCode(request.storage_id()));

            if (list->error_code() != proto::router::kErrorOk)
                return reply;

            QList<proto::router::SyncRecord> newer;
            for (const proto::router::SyncRecord& record : std::as_const(records_))
            {
                if (record.type() == request.type() && record.revision() > request.revision())
                    newer.append(record);
            }

            std::sort(newer.begin(), newer.end(),
                      [](const proto::router::SyncRecord& first, const proto::router::SyncRecord& second)
            {
                return first.revision() < second.revision();
            });

            const qint64 count = std::min<qint64>(request.count(), page_size_);

            for (const proto::router::SyncRecord& record : std::as_const(newer))
            {
                if (list->record_size() >= count)
                {
                    list->set_has_more(true);
                    break;
                }

                list->add_record()->CopyFrom(record);
            }

            list->set_revision(revision_);
            list->set_current_time(kRouterTime);
        }
        else if (message.has_record_request())
        {
            const proto::router::SyncRecordRequest& request = message.record_request();

            proto::router::SyncRecordResult* result = reply.mutable_record_result();
            result->set_request_id(request.request_id());
            result->set_error_code(errorCode(request.storage_id()));

            if (result->error_code() != proto::router::kErrorOk)
                return reply;

            for (const proto::router::SyncRecord& record : request.record())
            {
                qsizetype index = 0;
                while (index < records_.size() &&
                       (records_[index].type() != record.type() || records_[index].id() != record.id()))
                {
                    ++index;
                }

                const qint64 stored_revision = index < records_.size() ? records_[index].revision() : 0;
                if (record.revision() != stored_revision)
                {
                    result->add_record_revision(0);
                    continue;
                }

                if (index == records_.size())
                    records_.emplaceBack();

                records_[index].CopyFrom(record);
                records_[index].set_revision(++revision_);
                records_[index].set_time(kRouterTime);

                result->add_record_revision(revision_);
            }

            result->set_revision(revision_);
        }
        else
        {
            ADD_FAILURE() << "Unexpected request";
        }

        return reply;
    }

    void writeStorageInfo(proto::router::SyncStorageInfo* info) const
    {
        if (!error_code_.empty())
        {
            info->set_error_code(error_code_);
            return;
        }

        if (storage_id_.empty())
        {
            info->set_error_code(proto::router::kErrorNotFound);
            return;
        }

        info->set_error_code(proto::router::kErrorOk);
        info->set_storage_id(storage_id_);
        info->set_revision(revision_);
        info->set_key_revision(key_revision_);
        info->mutable_key()->CopyFrom(wrapped_key_);
        info->set_reset(storage_reset_);
    }

    std::string errorCode(const std::string& storage_id) const
    {
        if (!error_code_.empty())
            return error_code_;

        return storage_id == storage_id_ ? proto::router::kErrorOk : proto::router::kErrorNotFound;
    }

    // Another device resets the storage: the router drops the records and holds a storage with
    // another id and the key the other device gave it.
    void resetStorage(const char* storage_id, const proto::router::SyncStorageKey& wrapped_key)
    {
        storage_id_ = storage_id;
        storage_reset_ = true;
        wrapped_key_.CopyFrom(wrapped_key);
        ++key_revision_;
        records_.clear();
        revision_ = 0;
    }

    // The key the router holds, opened with a password of the account. Empty when it does not open.
    SecureByteArray routerKey(const char* password) const
    {
        SecureByteArray key;
        if (SyncCrypto::unwrapStorageKey(wrapped_key_, SecureString(QString(password)), &key) !=
            SyncCrypto::Result::SUCCESS)
        {
            return SecureByteArray();
        }

        return key;
    }

    // The router tells the device that the records have changed.
    void sendStorageNotification()
    {
        proto::router::RouterToSync notification;
        notification.mutable_storage_notification()->set_revision(revision_);
        worker_.onMessageReceived(serialize(notification));
    }

    // The other device sends what it has changed. The router tells the device of what it stored,
    // unless |notify| is off.
    void sendPhoneRecords(bool notify = true)
    {
        sendRecordsOf(phone_, notify);
    }

    void receivePhoneRecords()
    {
        receiveRecordsOf(phone_);
    }

    void sendPhoneProfiles()
    {
        sendRecordsOf(phone_profiles_, true);
    }

    void receivePhoneProfiles()
    {
        receiveRecordsOf(phone_profiles_);
    }

    void sendRecordsOf(SyncRecords& records, bool notify)
    {
        proto::router::SyncToRouter message;

        proto::router::SyncRecordRequest* request = message.mutable_record_request();
        ASSERT_EQ(records.readUnsyncedRecords(request), SyncRecords::SyncResult::DONE);
        request->set_storage_id(kStorageId);

        const qint64 revision = revision_;
        ASSERT_EQ(records.confirmSentRecords(routerReply(message).record_result()), SyncRecords::SyncResult::DONE);

        if (notify && revision_ != revision)
            sendStorageNotification();
    }

    void receiveRecordsOf(SyncRecords& records)
    {
        proto::router::SyncToRouter message;

        proto::router::SyncRecordListRequest* request = message.mutable_record_list_request();
        request->set_storage_id(kStorageId);
        request->set_type(records.type());
        request->set_revision(records.receivedRevision());
        request->set_count(proto::router::kMaxSyncPageSize);

        ASSERT_EQ(records.storeReceivedRecords(routerReply(message).record_list()), SyncRecords::SyncResult::DONE);
    }

    static CredentialConfig addCredential(Database& db, const QString& username)
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername(username);
        credential.setPassword(SecureString(QString("secret")));

        EXPECT_TRUE(CredentialStore(db).add(credential));
        return credential;
    }

    static void modifyCredential(Database& db, const QString& guid, const QString& username)
    {
        CredentialStore store(db);

        CredentialConfig credential;
        ASSERT_EQ(store.findByGuid(guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(store.modify(credential));
    }

    // The name of the user of the record, or an empty string when the device has no such record.
    static QString storedUsername(Database& db, const QString& guid)
    {
        CredentialConfig credential;
        if (CredentialStore(db).findByGuid(guid, &credential) != Database::FindResult::FOUND)
            return QString();

        return credential.username();
    }

    static QList<CredentialStore::Unsynced> unsyncedCredentials(Database& db)
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore(db).unsynced(&credentials), Database::ReadResult::OK);
        return credentials;
    }

    static ProfileConfig addProfile(Database& db, const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);

        EXPECT_TRUE(ProfileStore(db).add(profile));
        return profile;
    }

    // The name of the profile, or an empty string when the device has no such profile.
    static QString storedProfileName(Database& db, const QString& guid)
    {
        ProfileConfig profile;
        if (ProfileStore(db).findByGuid(guid, &profile) != Database::FindResult::FOUND)
            return QString();

        return profile.name();
    }

    QTemporaryDir dir_;
    Database phone_db_;
    const SecureByteArray key_ = storageKey().key;
    SyncCredentials phone_{ phone_db_, kStorageId, key_ };
    SyncProfiles phone_profiles_{ phone_db_, kStorageId, key_ };
    SyncWorker worker_;

    std::string storage_id_ = kStorageId;
    bool storage_reset_ = false;
    proto::router::SyncStorageKey wrapped_key_ = storageKey().wrapped;
    qint64 key_revision_ = 1;
    std::string error_code_;
    QList<proto::router::SyncRecord> records_;
    qint64 revision_ = 0;
    qint64 page_size_ = proto::router::kMaxSyncPageSize;
    int storage_info_request_count_ = 0;
    int storage_key_request_count_ = 0;
    int list_request_count_ = 0;
    int record_request_count_ = 0;
    int profile_list_request_count_ = 0;
    int profile_record_request_count_ = 0;
    int router_host_list_request_count_ = 0;
    int router_host_record_request_count_ = 0;
    QList<proto::router::SyncDataType> list_request_types_;
    int credentials_changed_count_ = 0;
    int profiles_changed_count_ = 0;
    int router_hosts_changed_count_ = 0;
    QList<std::pair<SyncWorker::Event, QString>> events_;
    std::function<void()> before_storage_key_request_;
    std::function<void()> before_listing_;
    std::function<void()> before_storing_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// Once the session with the router has started, the device asks for the storage. The key of the
// storage opens with the password of the account and is the one the device holds, so the device
// asks for the records and sends what it has not sent yet. The router stores the record, and the
// other device receives it.
TEST_F(SyncWorkerTest, NewRecordGoesToTheRouterAndToTheOtherDevice)
{
    Database& db = Database::instance();
    const QString guid = addCredential(db, "user").guid();

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());

    receivePhoneRecords();

    EXPECT_EQ(storedUsername(phone_db_, guid), QString("user"));
    EXPECT_TRUE(unsyncedCredentials(phone_db_).isEmpty());
    EXPECT_EQ(SyncStore(phone_db_).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 1);
}

//--------------------------------------------------------------------------------------------------
// A device that has nothing new asks the router for the records and sends none. When the router
// tells it that the records have changed, it receives the record the other device has made.
TEST_F(SyncWorkerTest, RecordOfTheOtherDeviceIsReceivedWhenTheRouterTellsOfIt)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 0);

    const QString guid = addCredential(phone_db_, "user").guid();
    sendPhoneRecords();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_EQ(storedUsername(db, guid), QString("user"));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
    EXPECT_EQ(SyncStore(db).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 1);
}

//--------------------------------------------------------------------------------------------------
// A record the other device has made changes the credentials of the device. The device announces
// it, so that a list of credentials on screen is read anew.
TEST_F(SyncWorkerTest, ChangeOfCredentialsIsAnnouncedWhenARecordIsReceived)
{
    worker_.onSessionStarted();
    ASSERT_EQ(credentials_changed_count_, 0);

    addCredential(phone_db_, "user");
    sendPhoneRecords();

    EXPECT_EQ(credentials_changed_count_, 1);
}

//--------------------------------------------------------------------------------------------------
// An exchange that brings no records from the router leaves the credentials of the device as
// they are: the record the device sends is its own. Nothing is announced.
TEST_F(SyncWorkerTest, ExchangeThatReceivesNothingAnnouncesNothing)
{
    Database& db = Database::instance();
    addCredential(db, "user");

    worker_.onSessionStarted();

    EXPECT_EQ(record_request_count_, 1);
    EXPECT_EQ(credentials_changed_count_, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, SentRecordIsNotReceivedBack)
{
    Database& db = Database::instance();
    addCredential(db, "user");

    worker_.onSessionStarted();

    ASSERT_EQ(record_request_count_, 1);
    EXPECT_EQ(SyncStore(db).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 1);

    worker_.onStartRecordExchange();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(credentials_changed_count_, 0);
}

//--------------------------------------------------------------------------------------------------
// The other device stores its record after this one has received the records and before it sends its own.
TEST_F(SyncWorkerTest, RecordTheOtherDeviceStoredMeanwhileIsStillReceived)
{
    Database& db = Database::instance();
    SyncStore sync_store(db);
    addCredential(db, "user");

    QString guid;

    before_storing_ = [this, &guid]()
    {
        guid = addCredential(phone_db_, "phone-user").guid();
        sendPhoneRecords(false);
    };

    worker_.onSessionStarted();

    ASSERT_EQ(record_request_count_, 1);
    EXPECT_EQ(sync_store.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);

    worker_.onStartRecordExchange();

    EXPECT_EQ(storedUsername(db, guid), QString("phone-user"));
    EXPECT_EQ(sync_store.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 2);
}

//--------------------------------------------------------------------------------------------------
// A record the user made after an exchange is over goes to the router as soon as the device is
// told to exchange, without a word from the router.
TEST_F(SyncWorkerTest, LocalChangeIsSentWithoutAWordFromTheRouter)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();
    ASSERT_EQ(record_request_count_, 0);

    const QString guid = addCredential(db, "user").guid();
    worker_.onStartRecordExchange();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());

    receivePhoneRecords();

    EXPECT_EQ(storedUsername(phone_db_, guid), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// A device that has no session with the router, or is attached to no storage, has no one to
// exchange with. Told to exchange, it asks the router nothing, and its change stays as it is.
TEST_F(SyncWorkerTest, LocalChangeWaitsWhileThereIsNoOneToExchangeWith)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onStartRecordExchange();

    EXPECT_EQ(storage_info_request_count_, 0);
    EXPECT_EQ(list_request_count_, 0);

    RouterStore store(db);
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();
    worker_.onStartRecordExchange();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The router hands the records out in pages. The device asks for the next page from where the
// previous one ended, until the router has no more.
TEST_F(SyncWorkerTest, RecordsAreReceivedPageByPage)
{
    Database& db = Database::instance();

    for (int i = 0; i < 5; ++i)
        addCredential(phone_db_, QString("user%1").arg(i));

    sendPhoneRecords();
    ASSERT_EQ(list_request_count_, 0);

    page_size_ = 2;
    worker_.onSessionStarted();

    EXPECT_EQ(list_request_count_, 3);
    EXPECT_EQ(record_request_count_, 0);

    QList<CredentialConfig> credentials;
    ASSERT_EQ(CredentialStore(db).list(&credentials), Database::ReadResult::OK);
    EXPECT_EQ(credentials.size(), 5);
    EXPECT_EQ(SyncStore(db).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 5);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The other device stored its change after this one had received the records and before it sent
// its own, and the router told this device of the change. The router refuses the record, and the
// device does what it was told of: receives what the router holds. Its own change gives way to it
// and is not sent again.
TEST_F(SyncWorkerTest, RefusedRecordGivesWayToWhatTheRouterHolds)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();

    worker_.onSessionStarted();
    receivePhoneRecords();

    ASSERT_EQ(record_request_count_, 1);
    ASSERT_EQ(storedUsername(phone_db_, guid), QString("user"));

    modifyCredential(db, guid, "laptop-user");
    modifyCredential(phone_db_, guid, "phone-user");

    before_storing_ = [this]()
    {
        sendPhoneRecords();
    };

    worker_.onSessionFinished();
    worker_.onSessionStarted();

    EXPECT_EQ(record_request_count_, 2);
    EXPECT_EQ(storedUsername(db, guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());

    receivePhoneRecords();

    EXPECT_EQ(storedUsername(phone_db_, guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(phone_db_).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The router refused a record, and the word of the change that caused it has not reached the
// device. The record is not sent again in the same exchange. It waits for the next one, which
// receives what the router holds, and the record gives way to it.
TEST_F(SyncWorkerTest, RefusedRecordWaitsForTheNextExchange)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();

    worker_.onSessionStarted();
    receivePhoneRecords();

    ASSERT_EQ(record_request_count_, 1);
    ASSERT_EQ(storedUsername(phone_db_, guid), QString("user"));

    modifyCredential(db, guid, "laptop-user");
    modifyCredential(phone_db_, guid, "phone-user");

    before_storing_ = [this]()
    {
        sendPhoneRecords(false);
    };

    worker_.onSessionFinished();
    worker_.onSessionStarted();

    EXPECT_EQ(record_request_count_, 2);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);

    sendStorageNotification();

    EXPECT_EQ(record_request_count_, 2);
    EXPECT_EQ(storedUsername(db, guid), QString("phone-user"));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An exchange asked for while one is running is not lost: once the running one is over, the
// device goes through the records again.
TEST_F(SyncWorkerTest, ExchangeAskedForDuringAnotherOneRunsAfterIt)
{
    before_listing_ = [this]()
    {
        sendStorageNotification();
    };

    worker_.onSessionStarted();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(record_request_count_, 0);
}

//--------------------------------------------------------------------------------------------------
// Synchronization is turned off on the device. Whatever the record of the router holds, the
// router is asked nothing.
TEST_F(SyncWorkerTest, DeviceWithSyncTurnedOffAsksTheRouterNothing)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    RouterConfig router = config();
    router.setSyncEnabled(false);
    ASSERT_TRUE(RouterStore(db).save(router));

    worker_.onSessionStarted();
    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 0);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Synchronization is turned off while the session runs. The device is detached from the storage
// and asks the router nothing more: neither a change made on the device nor a word from the router
// starts an exchange.
TEST_F(SyncWorkerTest, SyncTurnedOffInTheRunningSessionDetachesTheDevice)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();
    ASSERT_EQ(list_request_count_, 1);

    RouterConfig router = config();
    router.setSyncEnabled(false);
    ASSERT_TRUE(RouterStore(db).save(router));
    worker_.onSyncEnabledChanged();

    addCredential(db, "user");
    worker_.onStartRecordExchange();
    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_TRUE(storedRouter().syncStorageId().isEmpty());
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// Synchronization is turned on while the session runs. The device does not wait for the next
// session: it asks for the storage, attaches to it and exchanges. Without a session the word of
// the change does nothing.
TEST_F(SyncWorkerTest, SyncTurnedOnInTheRunningSessionAttachesTheDevice)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();

    worker_.onSyncEnabledChanged();
    ASSERT_EQ(storage_info_request_count_, 0);

    RouterConfig router = config();
    router.setSyncEnabled(false);

    RouterStore store(db);
    ASSERT_TRUE(store.save(router));

    worker_.onSessionStarted();
    ASSERT_EQ(storage_info_request_count_, 0);

    ASSERT_TRUE(store.save(config()));
    worker_.onSyncEnabledChanged();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_EQ(storedRouter().syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());

    receivePhoneRecords();

    EXPECT_EQ(storedUsername(phone_db_, guid), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// The router holds no storage of the user. The device makes a key, wraps it with the password of
// the account and has the router create the storage. It keeps the storage and the key in the record
// of the router and goes on to the exchange.
TEST_F(SyncWorkerTest, StorageIsCreatedWhenTheRouterHoldsNone)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    ASSERT_EQ(RouterStore(db).setSyncStorage(config(), QByteArray(), SecureByteArray(), false),
              Database::SyncResult::DONE);
    storage_id_.clear();

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 1);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_FALSE(stored.syncStorageKey().isEmpty());
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_EQ(records_.size(), 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A device that is attached to no storage finds one on the router. It opens the key with the
// password of the account, keeps the storage and the key in the record of the router and goes on
// to the exchange. The router is sent no key.
TEST_F(SyncWorkerTest, DeviceTakesTheKeyOfTheStorageTheRouterHolds)
{
    Database& db = Database::instance();

    const QString guid = addCredential(phone_db_, "user").guid();
    sendPhoneRecords();

    ASSERT_EQ(RouterStore(db).setSyncStorage(config(), QByteArray(), SecureByteArray(), false),
              Database::SyncResult::DONE);

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 0);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == key_);

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(storedUsername(db, guid), QString("user"));
}

//--------------------------------------------------------------------------------------------------
// Another device created the storage after this one was told that there is none. The router
// refuses to create the second one, and the device asks for the storage again and takes the key of
// the one that is there.
TEST_F(SyncWorkerTest, StorageCreatedByAnotherDeviceMeanwhileIsTaken)
{
    ASSERT_EQ(RouterStore().setSyncStorage(config(), QByteArray(), SecureByteArray(), false),
              Database::SyncResult::DONE);
    storage_id_.clear();

    before_storage_key_request_ = [this]()
    {
        storage_id_ = kStorageId;
    };

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 1);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == key_);

    EXPECT_EQ(list_request_count_, 1);
}

//--------------------------------------------------------------------------------------------------
// The router has lost the storage the device was attached to. The device has the router create
// one, not as a reset, and fills it from its own records.
TEST_F(SyncWorkerTest, StorageThatIsGoneIsCreatedFromTheRecordsOfTheDevice)
{
    Database& db = Database::instance();

    ASSERT_EQ(RouterStore(db).setSyncStorage(config(), kAnotherStorageId, key_, false), Database::SyncResult::DONE);

    const QString guid = addCredential(db, "user").guid();
    storage_id_.clear();

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 1);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_FALSE(stored.isSyncStorageResetPending());
    EXPECT_FALSE(storage_reset_);

    EXPECT_EQ(storedUsername(db, guid), QString("user"));
    EXPECT_EQ(records_.size(), 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The router has lost the storage the device was attached to, and the other device has made a new
// one in its place. The device keeps its records: it takes the records of the other device and
// sends its own.
TEST_F(SyncWorkerTest, DeviceKeepsItsRecordsInTheStorageMadeInPlaceOfALostOne)
{
    Database& db = Database::instance();

    ASSERT_EQ(RouterStore(db).setSyncStorage(config(), kAnotherStorageId, key_, false), Database::SyncResult::DONE);
    const QString guid = addCredential(db, "user").guid();

    const QString phone_guid = addCredential(phone_db_, "phone-user").guid();
    sendPhoneRecords(false);

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(storedRouter().syncStorageId(), QByteArray(kStorageId));

    EXPECT_EQ(storedUsername(db, guid), QString("user"));
    EXPECT_EQ(storedUsername(db, phone_guid), QString("phone-user"));
    EXPECT_EQ(records_.size(), 2);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Both devices found the storage lost, and the other device made a new one first. The device takes
// that one and keeps its records instead of resetting it.
TEST_F(SyncWorkerTest, StorageTheOtherDeviceMadeInPlaceOfALostOneIsNotReset)
{
    Database& db = Database::instance();

    ASSERT_EQ(RouterStore(db).setSyncStorage(config(), kAnotherStorageId, key_, false), Database::SyncResult::DONE);
    const QString guid = addCredential(db, "user").guid();
    const QString phone_guid = addCredential(phone_db_, "phone-user").guid();
    storage_id_.clear();

    before_storage_key_request_ = [this]()
    {
        storage_id_ = kStorageId;
        sendPhoneRecords(false);
    };

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kStorageId));
    EXPECT_FALSE(storage_reset_);

    EXPECT_EQ(storedUsername(db, guid), QString("user"));
    EXPECT_EQ(storedUsername(db, phone_guid), QString("phone-user"));
    EXPECT_EQ(records_.size(), 2);
}

//--------------------------------------------------------------------------------------------------
// The user told the device to reset the storage, and the router has lost it meanwhile. The storage
// the device makes stands for the reset, so that the other devices give up their records.
TEST_F(SyncWorkerTest, StorageMadeForAResetInPlaceOfALostOneTellsOfTheReset)
{
    Database& db = Database::instance();

    RouterStore store(db);
    ASSERT_EQ(store.setSyncStorage(config(), kAnotherStorageId, key_, false), Database::SyncResult::DONE);
    ASSERT_TRUE(store.setSyncStorageResetPending(config()));
    addCredential(db, "user");
    storage_id_.clear();

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kStorageId));
    EXPECT_TRUE(storage_reset_);
    EXPECT_FALSE(storedRouter().isSyncStorageResetPending());
    EXPECT_EQ(records_.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The password of the account has changed, and the key on the router is still wrapped with the
// old one. The device holds the key of this storage, so it wraps the key with the new password and
// sends it over the revision of the key it was told. Then it exchanges as usual.
TEST_F(SyncWorkerTest, KeyIsWrappedAnewAfterThePasswordOfTheAccountHasChanged)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    ASSERT_TRUE(RouterStore(db).save(config(kNewPassword)));

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(key_revision_, 2);
    EXPECT_TRUE(routerKey(kNewPassword) == key_);

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The user of the router was renamed, and the record of the router was saved with the new name.
// The device signs in as the same user, so it stays attached to the storage. It receives and
// sends only what is new, as if the name had not changed.
TEST_F(SyncWorkerTest, RenamedUserGoesOnWithTheSameStorage)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();
    worker_.onSessionStarted();

    ASSERT_EQ(record_request_count_, 1);
    ASSERT_TRUE(unsyncedCredentials(db).isEmpty());

    worker_.onSessionFinished();

    RouterConfig renamed = config();
    renamed.setUsername("renamed-user");
    ASSERT_TRUE(RouterStore(db).save(renamed));

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_EQ(records_.size(), 1);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.username(), QString("renamed-user"));
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == key_);

    EXPECT_EQ(storedUsername(db, guid), QString("user"));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The router renames a user only together with a new password. The record of the router is saved
// with the new name and the new password, and the key on the router is still wrapped with the old
// one. The device has kept the key of the storage through the change of the name, so it wraps the
// key with the new password, as it does when the password alone has changed.
TEST_F(SyncWorkerTest, KeyIsWrappedAnewAfterTheUserIsRenamedWithAnotherPassword)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    RouterConfig renamed = config(kNewPassword);
    renamed.setUsername("renamed-user");
    ASSERT_TRUE(RouterStore(db).save(renamed));

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(key_revision_, 2);
    EXPECT_TRUE(routerKey(kNewPassword) == key_);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == key_);

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Another device wrapped the key anew after this one was told its revision. The router refuses
// the key of this device, and the device needs nothing more: it holds the key of the storage and
// goes on to the exchange.
TEST_F(SyncWorkerTest, KeyWrappedAnewByAnotherDeviceMeanwhileIsLeftAsItIs)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    ASSERT_TRUE(RouterStore(db).save(config(kNewPassword)));

    before_storage_key_request_ = [this]()
    {
        ++key_revision_;
    };

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(key_revision_, 2);
    EXPECT_EQ(wrapped_key_.data(), storageKey().wrapped.data());

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The key on the router does not open with the password of the account, and the device holds no
// key of this storage to wrap anew. It stays out of the synchronization. A word from the router
// makes it ask for the storage again, and as long as the key is the same, it does not open.
TEST_F(SyncWorkerTest, DeviceWithoutTheKeyWaitsWhenTheKeyDoesNotOpen)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    RouterStore store(db);
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();

    EXPECT_EQ(storage_info_request_count_, 1);

    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_TRUE(storedRouter().syncStorageId().isEmpty());
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);
}

//--------------------------------------------------------------------------------------------------
// A device that waits for the key does not wait for the next session. Another device wraps the
// key with the new password of the account, and the router tells of it: the device asks for the
// storage again, opens the key and exchanges.
TEST_F(SyncWorkerTest, DeviceWithoutTheKeyAttachesOnceTheKeyIsWrappedAnew)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    RouterStore store(db);
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();

    ASSERT_EQ(storage_info_request_count_, 1);
    ASSERT_EQ(list_request_count_, 0);

    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key_, SecureString(QString(kNewPassword)), &wrapped_key_));
    ++key_revision_;
    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(storedRouter().syncStorageKey() == key_);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Another device has reset the storage while the session was running, so the router holds a
// storage with another id and another key. The router refuses the old storage, and the device asks
// for the storage again, opens the new key and keeps it instead of the old one. The new storage is
// filled from the device that made it, so this one gives up its records, sends nothing and tells
// that the credentials have changed.
TEST_F(SyncWorkerTest, StorageResetFromAnotherDeviceIsTakenInTheRunningSession)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();

    worker_.onSessionStarted();

    ASSERT_EQ(list_request_count_, 1);
    ASSERT_EQ(record_request_count_, 1);
    ASSERT_TRUE(unsyncedCredentials(db).isEmpty());
    ASSERT_EQ(credentials_changed_count_, 0);

    const StorageKey another = makeStorageKey(kPassword);
    resetStorage(kAnotherStorageId, another.wrapped);
    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(list_request_count_, 3);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(records_.isEmpty());

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == another.key);

    EXPECT_TRUE(storedUsername(db, guid).isEmpty());
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
    EXPECT_EQ(credentials_changed_count_, 1);
}

//--------------------------------------------------------------------------------------------------
// The router failed to tell of the storage. The device stays out of the synchronization, and a
// change made on the device starts nothing. Once the router tells of a change, the device asks for
// the storage again and exchanges without waiting for the interval of the exchange.
TEST_F(SyncWorkerTest, DeviceLeftOutByAnErrorOfTheRouterComesBackWhenTheRouterTellsOfAChange)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    error_code_ = proto::router::kErrorInternalError;
    worker_.onSessionStarted();

    error_code_.clear();
    worker_.onStartRecordExchange();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);

    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An error of the router ends the exchange where it is. Nothing is lost: the next exchange takes
// it from the start.
TEST_F(SyncWorkerTest, ErrorOfTheRouterStopsTheExchangeUntilTheNextOne)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();
    ASSERT_EQ(list_request_count_, 1);

    addCredential(db, "user");

    error_code_ = proto::router::kErrorInternalError;
    sendStorageNotification();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);

    error_code_.clear();
    sendStorageNotification();

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Synchronization was turned off while a request was on its way. The device is not attached to
// the storage any more, so the answer changes nothing: the records of the storage are not stored,
// and the exchange ends where it is.
TEST_F(SyncWorkerTest, AnswerThatComesAfterSyncIsTurnedOffChangesNothing)
{
    Database& db = Database::instance();

    const QString guid = addCredential(phone_db_, "user").guid();
    sendPhoneRecords();

    before_listing_ = [&db]()
    {
        RouterConfig router = config();
        router.setSyncEnabled(false);
        ASSERT_TRUE(RouterStore(db).save(router));
    };

    worker_.onSessionStarted();

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_TRUE(storedUsername(db, guid).isEmpty());
    EXPECT_EQ(SyncStore(db).receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 0);
    EXPECT_TRUE(storedRouter().syncStorageId().isEmpty());

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::STORAGE_ATTACHED);
}

//--------------------------------------------------------------------------------------------------
// The session with the router ended while a request was on its way. The answer is dropped, and
// the router is asked nothing until the next session, which takes the exchange from the start.
TEST_F(SyncWorkerTest, ExchangeEndsWithTheSessionAndStartsWithTheNextOne)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    before_listing_ = [this]()
    {
        worker_.onSessionFinished();
    };

    worker_.onSessionStarted();
    sendStorageNotification();

    EXPECT_EQ(list_request_count_, 1);
    EXPECT_EQ(record_request_count_, 0);
    EXPECT_EQ(unsyncedCredentials(db).size(), 1);

    worker_.onSessionStarted();

    EXPECT_EQ(list_request_count_, 2);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, ExchangeRunsAgainOnceTheIntervalHasPassed)
{
    worker_.onSessionStarted();
    ASSERT_EQ(list_request_count_, 1);

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(9));
    EXPECT_EQ(list_request_count_, 1);

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));
    EXPECT_EQ(list_request_count_, 2);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, ExchangeStoppedByAnErrorOfTheRouterIsRetriedOnceTheIntervalHasPassed)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();
    addCredential(db, "user");

    error_code_ = proto::router::kErrorInternalError;
    sendStorageNotification();
    ASSERT_EQ(record_request_count_, 0);

    error_code_.clear();
    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));

    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, StorageTheRouterFailedToTellOfIsAskedForOnceTheIntervalHasPassed)
{
    Database& db = Database::instance();
    addCredential(db, "user");

    error_code_ = proto::router::kErrorInternalError;
    worker_.onSessionStarted();
    ASSERT_EQ(list_request_count_, 0);

    error_code_.clear();
    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The record of the router that is not read at the start of the session is read again soon.
TEST_F(SyncWorkerTest, RouterNotReadAtTheStartOfTheSessionIsReadAgainBeforeTheIntervalHasPassed)
{
    Database& db = Database::instance();
    addCredential(db, "user");

    DataCryptor& cryptor = DataCryptor::instance();
    const SecureByteArray key = cryptor.key();

    cryptor.setKey(SecureByteArray(Random::byteArray(32)));
    worker_.onSessionStarted();
    cryptor.setKey(key);

    ASSERT_EQ(storage_info_request_count_, 0);

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Seconds(14));
    EXPECT_EQ(storage_info_request_count_, 0);

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Seconds(15));

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::STORAGE_ATTACHED);
}

//--------------------------------------------------------------------------------------------------
// The record of the router that is not read with the key of the storage is read again soon.
TEST_F(SyncWorkerTest, RouterNotReadWithTheKeyOfTheStorageIsReadAgainBeforeTheIntervalHasPassed)
{
    Database& db = Database::instance();
    addCredential(db, "user");

    error_code_ = proto::router::kErrorInternalError;
    worker_.onSessionStarted();
    ASSERT_EQ(storage_info_request_count_, 1);

    error_code_.clear();
    events_.clear();

    DataCryptor& cryptor = DataCryptor::instance();
    const SecureByteArray key = cryptor.key();

    cryptor.setKey(SecureByteArray(Random::byteArray(32)));
    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));
    cryptor.setKey(key);

    ASSERT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_TRUE(events_.isEmpty());

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Seconds(15));

    EXPECT_EQ(storage_info_request_count_, 3);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::STORAGE_ATTACHED);
}

//--------------------------------------------------------------------------------------------------
// Only the user can make the key open, by signing in with the current password of the account.
TEST_F(SyncWorkerTest, KeyThatDoesNotOpenIsNotAskedForAgainOnceTheIntervalHasPassed)
{
    RouterStore store;
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();
    ASSERT_EQ(storage_info_request_count_, 1);

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));

    EXPECT_EQ(storage_info_request_count_, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, NothingIsAskedOnceTheIntervalHasPassedWithoutASession)
{
    worker_.onSessionStarted();
    worker_.onSessionFinished();

    SyncWorkerTestPeer::runTimer(worker_, Clock::now() + Minutes(10));

    EXPECT_EQ(storage_info_request_count_, 1);
    EXPECT_EQ(list_request_count_, 1);
}

//--------------------------------------------------------------------------------------------------
// A device that has attached to the storage tells of it, so that the user sees the
// synchronization at work.
TEST_F(SyncWorkerTest, AttachedStorageIsAnnounced)
{
    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::STORAGE_ATTACHED);
}

//--------------------------------------------------------------------------------------------------
// A device that cannot open the key and holds none tells of it: the user is the one who can do
// something about it.
TEST_F(SyncWorkerTest, KeyThatDoesNotOpenIsAnnounced)
{
    RouterStore store;
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::STORAGE_KEY_NOT_OPENED);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, KeyThatCannotBeReadIsAnnouncedAsAnInvalidAnswerOfTheRouter)
{
    wrapped_key_.set_salt("short");

    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::INVALID_ROUTER_ANSWER);
    EXPECT_EQ(list_request_count_, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, StorageIdOfWrongSizeIsRefused)
{
    storage_id_ = "0123456789abcde";

    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::INVALID_ROUTER_ANSWER);
    EXPECT_EQ(list_request_count_, 0);
    EXPECT_EQ(storedRouter().syncStorageId(), QByteArray(kStorageId));
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, AnotherKeyForTheSameStorageIsRefused)
{
    wrapped_key_.CopyFrom(makeStorageKey(kPassword).wrapped);

    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::INVALID_ROUTER_ANSWER);
    EXPECT_EQ(storage_key_request_count_, 0);
    EXPECT_EQ(list_request_count_, 0);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kStorageId));
    EXPECT_TRUE(stored.syncStorageKey() == key_);
}

//--------------------------------------------------------------------------------------------------
// An error the router answers with is told with its code, whether it comes when the device asks
// for the storage or in the middle of an exchange.
TEST_F(SyncWorkerTest, ErrorOfTheRouterIsAnnounced)
{
    error_code_ = proto::router::kErrorInternalError;
    worker_.onSessionStarted();

    ASSERT_EQ(events_.size(), 1);
    EXPECT_EQ(events_[0].first, SyncWorker::Event::ROUTER_ERROR);
    EXPECT_EQ(events_[0].second, QString(proto::router::kErrorInternalError));

    error_code_.clear();
    sendStorageNotification();

    ASSERT_EQ(events_.size(), 2);
    EXPECT_EQ(events_[1].first, SyncWorker::Event::STORAGE_ATTACHED);

    error_code_ = proto::router::kErrorInternalError;
    sendStorageNotification();

    ASSERT_EQ(events_.size(), 3);
    EXPECT_EQ(events_[2].first, SyncWorker::Event::ROUTER_ERROR);
    EXPECT_EQ(events_[2].second, QString(proto::router::kErrorInternalError));
}

//--------------------------------------------------------------------------------------------------
// The user tells the device to reset the storage. The device asks for the storage, makes a new
// key, wraps it with the password of the account and has the router replace the storage. The
// records the router held are gone, and the device sends its own again, under the new key.
TEST_F(SyncWorkerTest, StorageIsResetWhenTheDeviceIsToldTo)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onSessionStarted();

    ASSERT_EQ(record_request_count_, 1);

    addCredential(phone_db_, "phone-user");
    sendPhoneRecords(false);

    ASSERT_EQ(records_.size(), 2);

    ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));
    worker_.onStorageResetPending();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_FALSE(stored.syncStorageKey() == key_);
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());

    ASSERT_EQ(records_.size(), 1);
    EXPECT_TRUE(SyncCrypto::decryptRecord(
        stored.syncStorageKey(), proto::router::SYNC_DATA_TYPE_CREDENTIALS, records_[0].id(),
        QByteArray::fromStdString(records_[0].data())).has_value());
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A device that cannot open the key and holds none is out of the synchronization, and no other
// device may ever come to wrap the key anew. Told to reset the storage, the device replaces it
// with one of its own key, attaches to it and sends its records.
TEST_F(SyncWorkerTest, DeviceWithoutTheKeyResetsTheStorage)
{
    Database& db = Database::instance();

    addCredential(db, "user");

    RouterStore store(db);
    ASSERT_TRUE(store.save(config(kNewPassword)));
    ASSERT_EQ(store.setSyncStorage(config(), QByteArray(), SecureByteArray(), false), Database::SyncResult::DONE);

    worker_.onSessionStarted();

    ASSERT_EQ(list_request_count_, 0);

    ASSERT_TRUE(store.setSyncStorageResetPending(config()));
    worker_.onStorageResetPending();

    EXPECT_EQ(storage_key_request_count_, 1);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(routerKey(kNewPassword) == stored.syncStorageKey());

    EXPECT_EQ(record_request_count_, 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Another device has reset the storage after this one asked for it. The router refuses to reset
// a storage it does not hold any more. The user told this device to reset the storage, so it asks
// for the storage again and resets the one the other device has made.
TEST_F(SyncWorkerTest, StorageResetByAnotherDeviceMeanwhileIsResetAgain)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onSessionStarted();

    const StorageKey another = makeStorageKey(kPassword);

    before_storage_key_request_ = [this, &another]()
    {
        resetStorage(kAnotherStorageId, another.wrapped);
    };

    ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));
    worker_.onStorageResetPending();

    EXPECT_EQ(storage_info_request_count_, 3);
    EXPECT_EQ(storage_key_request_count_, 2);

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_FALSE(stored.syncStorageKey() == another.key);
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());

    EXPECT_EQ(records_.size(), 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, ResetAskedForAgainDuringResetIsDoneOnce)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onSessionStarted();

    before_storage_key_request_ = [this]()
    {
        worker_.onStorageResetPending();
    };

    ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));
    worker_.onStorageResetPending();

    EXPECT_EQ(storage_info_request_count_, 2);
    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());
    EXPECT_EQ(records_.size(), 1);
}

//--------------------------------------------------------------------------------------------------
// The reset is asked for while the device wraps the key anew with the new password.
TEST_F(SyncWorkerTest, ResetAskedForDuringAttachIsNotLost)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    ASSERT_TRUE(RouterStore(db).save(config(kNewPassword)));

    before_storage_key_request_ = [&db]()
    {
        ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));
    };

    worker_.onSessionStarted();

    ASSERT_EQ(storage_key_request_count_, 1);
    ASSERT_EQ(storage_id_, std::string(kStorageId));
    ASSERT_TRUE(storedRouter().isSyncStorageResetPending());

    worker_.onStorageResetPending();

    EXPECT_EQ(storage_key_request_count_, 2);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(routerKey(kNewPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
// A reset the device asked for stays asked for until the device is attached to the storage it
// made. A session that ended before that leaves the reset to the next one.
TEST_F(SyncWorkerTest, ResetLeftUnfinishedIsDoneInTheNextSession)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onSessionStarted();

    ASSERT_EQ(records_.size(), 1);

    worker_.onSessionFinished();
    ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());

    EXPECT_EQ(records_.size(), 1);
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A backup restored on the device leaves the reset of the storage pending and the device attached
// to no storage. The next session resets the storage the router holds, and the restored records
// are what the new storage is made of.
TEST_F(SyncWorkerTest, PendingResetOfADeviceWithoutAStorageIsDone)
{
    Database& db = Database::instance();

    const QString replaced_guid = addCredential(db, "user").guid();
    worker_.onSessionStarted();

    ASSERT_EQ(records_.size(), 1);

    worker_.onSessionFinished();

    CredentialConfig restored;
    restored.setDisplayName("office");
    restored.setUsername("restored");
    restored.setPassword(SecureString(QString("secret")));

    ASSERT_TRUE(db.import(config(), {}, {}, {}, { restored }, {}));
    ASSERT_TRUE(storedRouter().isSyncStorageResetPending());
    ASSERT_TRUE(storedRouter().syncStorageId().isEmpty());

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));

    const RouterConfig stored = storedRouter();
    EXPECT_EQ(stored.syncStorageId(), QByteArray(kAnotherStorageId));
    EXPECT_TRUE(routerKey(kPassword) == stored.syncStorageKey());
    EXPECT_FALSE(stored.isSyncStorageResetPending());

    ASSERT_EQ(records_.size(), 1);
    EXPECT_NE(records_[0].id(), replaced_guid.toStdString());
    EXPECT_EQ(storedUsername(db, QString::fromStdString(records_[0].id())), QString("restored"));
    EXPECT_TRUE(unsyncedCredentials(db).isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, PendingResetComingWithoutASessionIsDoneInTheNextSession)
{
    Database& db = Database::instance();

    addCredential(db, "user");
    worker_.onSessionStarted();
    worker_.onSessionFinished();

    const auto info_request_count = storage_info_request_count_;
    ASSERT_TRUE(RouterStore(db).setSyncStorageResetPending(config()));

    worker_.onStorageResetPending();

    EXPECT_EQ(storage_info_request_count_, info_request_count);
    EXPECT_EQ(storage_key_request_count_, 0);

    worker_.onSessionStarted();

    EXPECT_EQ(storage_key_request_count_, 1);
    EXPECT_EQ(storage_id_, std::string(kAnotherStorageId));
    EXPECT_FALSE(storedRouter().isSyncStorageResetPending());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, PendingResetIsNotDoneWhileSyncIsOff)
{
    RouterConfig router = config();
    router.setSyncEnabled(false);
    ASSERT_TRUE(RouterStore().save(router));

    worker_.onSessionStarted();
    worker_.onStorageResetPending();

    EXPECT_EQ(storage_info_request_count_, 0);
    EXPECT_EQ(storage_key_request_count_, 0);
}

//--------------------------------------------------------------------------------------------------
// An exchange takes the credentials first, the profiles after them and the router hosts last, so
// that the records a host links are on the device before the host comes. Each type is received
// before it is sent.
TEST_F(SyncWorkerTest, ExchangeTakesTheCredentialsThenTheProfilesThenTheRouterHosts)
{
    Database& db = Database::instance();
    addCredential(db, "user");
    addProfile(db, "office");

    RouterHostConfig host;
    host.setHostId(100500);
    host.setUsername("user");
    host.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(RouterHostStore(db).add(host));

    worker_.onSessionStarted();

    const QList<proto::router::SyncDataType> expected_types = {
        proto::router::SYNC_DATA_TYPE_CREDENTIALS, proto::router::SYNC_DATA_TYPE_PROFILES,
        proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS };

    EXPECT_EQ(list_request_types_, expected_types);
    EXPECT_EQ(record_request_count_, 1);
    EXPECT_EQ(profile_record_request_count_, 1);
    EXPECT_EQ(router_host_record_request_count_, 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncWorkerTest, ProfileGoesToTheRouterAndToTheOtherDevice)
{
    const QString guid = addProfile(Database::instance(), "office").guid();

    worker_.onSessionStarted();

    EXPECT_EQ(profile_list_request_count_, 1);
    EXPECT_EQ(profile_record_request_count_, 1);
    EXPECT_EQ(record_request_count_, 0);

    receivePhoneProfiles();

    EXPECT_EQ(storedProfileName(phone_db_, guid), QString("office"));
    EXPECT_EQ(SyncStore(phone_db_).receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 1);
}

//--------------------------------------------------------------------------------------------------
// A profile the other device has made is received and announced as a change of the profiles only.
TEST_F(SyncWorkerTest, ProfileOfTheOtherDeviceIsReceivedAndAnnounced)
{
    Database& db = Database::instance();

    worker_.onSessionStarted();
    ASSERT_EQ(profiles_changed_count_, 0);

    const QString guid = addProfile(phone_db_, "office").guid();
    sendPhoneProfiles();

    EXPECT_EQ(storedProfileName(db, guid), QString("office"));
    EXPECT_EQ(SyncStore(db).receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 1);
    EXPECT_EQ(profiles_changed_count_, 1);
    EXPECT_EQ(credentials_changed_count_, 0);
}

//--------------------------------------------------------------------------------------------------
// The revisions of the router are shared by the types, so the records the device sent are passed by in both.
TEST_F(SyncWorkerTest, SentProfileIsNotReceivedBack)
{
    Database& db = Database::instance();
    SyncStore sync_store(db);
    addCredential(db, "user");
    addProfile(db, "office");

    worker_.onSessionStarted();

    EXPECT_EQ(sync_store.receivedRevision(proto::router::SYNC_DATA_TYPE_CREDENTIALS), 1);
    EXPECT_EQ(sync_store.receivedRevision(proto::router::SYNC_DATA_TYPE_PROFILES), 2);

    worker_.onStartRecordExchange();

    EXPECT_EQ(profile_list_request_count_, 2);
    EXPECT_EQ(credentials_changed_count_, 0);
    EXPECT_EQ(profiles_changed_count_, 0);
}

//--------------------------------------------------------------------------------------------------
// A credential the router refused waits for the next exchange, and the profiles are sent meanwhile.
TEST_F(SyncWorkerTest, RefusedCredentialDoesNotHoldBackTheProfiles)
{
    Database& db = Database::instance();

    const QString guid = addCredential(db, "user").guid();

    worker_.onSessionStarted();
    receivePhoneRecords();
    ASSERT_EQ(storedUsername(phone_db_, guid), QString("user"));

    modifyCredential(db, guid, "laptop-user");
    modifyCredential(phone_db_, guid, "phone-user");
    const QString profile_guid = addProfile(db, "office").guid();

    before_storing_ = [this]()
    {
        sendPhoneRecords(false);
    };

    worker_.onStartRecordExchange();

    EXPECT_EQ(unsyncedCredentials(db).size(), 1);
    EXPECT_EQ(profile_record_request_count_, 1);

    receivePhoneProfiles();

    EXPECT_EQ(storedProfileName(phone_db_, profile_guid), QString("office"));
}
