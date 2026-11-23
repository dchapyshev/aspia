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

#include <QElapsedTimer>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <unordered_map>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/credential_store.h"
#include "client/storage/local_group_store.h"
#include "client/storage/local_host_store.h"
#include "client/storage/profile_store.h"
#include "client/storage/router_host_store.h"
#include "client/storage/router_store.h"
#include "client/storage/sync_store.h"
#include "client/workers/sync_worker.h"
#include "proto/router.h"
#include "router/storage/sync_storage.h"

namespace {

const char kUserGuid[] = "11111111-1111-4111-8111-111111111111";
const HostId kHostId = 100500;

// A device of the user: its database and the worker of the application. The worker takes the
// database of the thread it is called in, so everything the device does runs in a thread of its own.
// The worker belongs to its own thread that never starts here, its slots are called directly.
class Device
{
public:
    Device(const QString& directory_path, const QString& name)
        : name_(name)
    {
        context_.moveToThread(&thread_);
        thread_.start();

        invoke([&]()
        {
            DatabaseTestPeer::setFilePath(directory_path + '/' + name + ".db3");
            ASSERT_TRUE(Database::instance().isValid());

            RouterConfig router;
            router.setAddress("router.example.com");
            router.setUsername("user");
            router.setPassword(SecureString(QString("secret")));
            router.setSyncEnabled(true);
            ASSERT_TRUE(RouterStore().save(router));

            worker_ = std::make_unique<SyncWorker>();

            QObject::connect(worker_.get(), &SyncWorker::sig_sendMessage, worker_.get(),
                             [this](quint8 channel_id, const QByteArray& buffer)
            {
                EXPECT_EQ(channel_id, proto::router::CHANNEL_ID_SYNC);
                sent_messages_.append(buffer);
            }, Qt::DirectConnection);

            QObject::connect(worker_.get(), &SyncWorker::sig_event, worker_.get(),
                             [this](SyncWorker::Event event, const QString& /* error_code */)
            {
                events_.append(event);
            }, Qt::DirectConnection);
        });
    }

    ~Device()
    {
        invoke([this]() { worker_.reset(); });

        thread_.quit();
        thread_.wait();
    }

    // Runs |work| in the thread of the device, where the stores open the database of the device.
    void run(const std::function<void()>& work)
    {
        invoke([&]()
        {
            ASSERT_EQ(Database::instance().guid(), name_);
            work();
        });
    }

    void startSession()
    {
        run([this]() { worker_->onSessionStarted(); });
    }

    void finishSession()
    {
        run([this]() { worker_->onSessionFinished(); });
    }

    // The user turns the synchronization on or off in the settings of the router.
    void setSyncEnabled(bool enabled)
    {
        run([&]()
        {
            RouterStore store;

            RouterConfig router;
            ASSERT_EQ(store.find(&router), Database::FindResult::FOUND);

            router.setSyncEnabled(enabled);
            ASSERT_TRUE(store.save(router));

            worker_->onSyncEnabledChanged();
        });
    }

    // The user tells the device to reset the storage.
    void resetStorage()
    {
        run([&]()
        {
            RouterStore store;

            RouterConfig router;
            ASSERT_EQ(store.find(&router), Database::FindResult::FOUND);
            ASSERT_TRUE(store.setSyncStorageResetPending(router));

            worker_->onStorageResetPending();
        });
    }

    // What the application does after the user has changed a record.
    void startExchange()
    {
        run([this]() { worker_->onStartRecordExchange(); });
    }

    void receive(const QByteArray& buffer)
    {
        run([&]() { worker_->onMessageReceived(buffer); });
    }

    QByteArray nextSentMessage() const
    {
        return sent_messages_.value(0);
    }

    bool takeSentMessage(QByteArray* buffer)
    {
        if (sent_messages_.isEmpty())
            return false;

        *buffer = sent_messages_.takeFirst();
        return true;
    }

    const QList<SyncWorker::Event>& events() const { return events_; }

private:
    void invoke(const std::function<void()>& work)
    {
        ASSERT_TRUE(QMetaObject::invokeMethod(&context_, work, Qt::BlockingQueuedConnection));
    }

    const QString name_;
    QThread thread_;
    QObject context_;
    std::unique_ptr<SyncWorker> worker_;
    QList<QByteArray> sent_messages_;
    QList<SyncWorker::Event> events_;

    Q_DISABLE_COPY_MOVE(Device)
};

// Two devices of one user and the router. The devices run the worker, the stores and the database
// of the application. The router is its storage of records, the one the application of the router
// runs. What stands between them is written here: answerSyncMessage() below repeats
// ClientOperator::readSyncMessage() and ClientWorker::onSyncChanged() of the router, and settle()
// carries the messages instead of the network.
class SyncExchangeTest : public testing::Test
{
protected:
    // The session of a device at the router.
    struct Session
    {
        Session(const QString& directory_path, const QString& name)
            : device(directory_path, name)
        {
            // Nothing
        }

        Device device;
        SyncStorage storage;
        QList<QByteArray> notifications;
    };

    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());
        SyncStorage::setDirectoryPathForTesting(dir_.filePath("router"));

        laptop_ = std::make_unique<Session>(dir_.path(), "laptop");
        phone_ = std::make_unique<Session>(dir_.path(), "phone");
    }

    void answerSyncMessage(Session& session, const QByteArray& buffer, QByteArray* answer)
    {
        proto::router::SyncToRouter message;
        ASSERT_TRUE(parse(buffer, &message));

        if (!session.storage.isValid())
        {
            ASSERT_TRUE(session.storage.open(kUserGuid));
        }

        proto::router::RouterToSync reply;
        bool changed = false;
        qint64 revision = 0;

        if (message.has_storage_info_request())
        {
            proto::router::SyncStorageInfo* info = reply.mutable_storage_info();
            info->set_request_id(message.storage_info_request().request_id());
            session.storage.readStorageInfo(info);
        }
        else if (message.has_storage_key_request())
        {
            const proto::router::SyncStorageKeyRequest& request = message.storage_key_request();

            proto::router::SyncStorageInfo* info = reply.mutable_storage_info();
            info->set_request_id(request.request_id());

            switch (request.mode())
            {
                case proto::router::SyncStorageKeyRequest::MODE_CREATE:
                    session.storage.createStorage(request, info);
                    break;

                case proto::router::SyncStorageKeyRequest::MODE_MODIFY:
                    session.storage.changeKey(request, info);
                    changed = info->error_code() == proto::router::kErrorOk;
                    break;

                case proto::router::SyncStorageKeyRequest::MODE_RESET:
                    session.storage.resetStorage(request, info);
                    changed = info->error_code() == proto::router::kErrorOk;
                    break;

                default:
                    info->set_error_code(proto::router::kErrorInvalidRequest);
                    break;
            }

            revision = info->revision();
        }
        else if (message.has_record_list_request())
        {
            proto::router::SyncRecordList* list = reply.mutable_record_list();
            list->set_request_id(message.record_list_request().request_id());
            session.storage.readRecords(message.record_list_request(), list);
        }
        else if (message.has_record_request())
        {
            proto::router::SyncRecordResult* result = reply.mutable_record_result();
            result->set_request_id(message.record_request().request_id());
            session.storage.storeRecords(message.record_request(), result, &changed);
            revision = result->revision();
        }
        else
        {
            FAIL() << "Unhandled sync message";
        }

        *answer = serialize(reply);

        if (!changed)
            return;

        proto::router::RouterToSync notification;
        notification.mutable_storage_notification()->set_revision(revision);

        for (Session* other : { laptop_.get(), phone_.get() })
        {
            if (other != &session && other->storage.isValid())
                other->notifications.append(serialize(notification));
        }
    }

    void readSyncMessage(Session& session, const QByteArray& buffer)
    {
        QByteArray answer;
        ASSERT_NO_FATAL_FAILURE(answerSyncMessage(session, buffer, &answer));

        session.device.receive(answer);
    }

    // The router takes the oldest request of the device, and the session breaks before the answer
    // reaches the device.
    void breakSessionAtRequest(Session& session)
    {
        QByteArray buffer;
        ASSERT_TRUE(session.device.takeSentMessage(&buffer));

        QByteArray answer;
        ASSERT_NO_FATAL_FAILURE(answerSyncMessage(session, buffer, &answer));

        session.device.finishSession();
        session.storage.close();
        session.notifications.clear();
    }

    // The device goes offline with nothing on its way to the router.
    static void endSession(Session& session)
    {
        session.device.finishSession();
        session.storage.close();
        session.notifications.clear();
    }

    // The session breaks before the oldest request of the device reaches the router.
    void breakSessionBeforeRequest(Session& session)
    {
        QByteArray buffer;
        ASSERT_TRUE(session.device.takeSentMessage(&buffer));

        session.device.finishSession();
        session.storage.close();
        session.notifications.clear();
    }

    // Carries the oldest request of the device to the router and the answer back.
    bool carryRequest(Session& session)
    {
        QByteArray buffer;
        if (!session.device.takeSentMessage(&buffer))
            return false;

        readSyncMessage(session, buffer);
        return true;
    }

    // Carries the requests of the device till the device is about to send records of |type|. The
    // request with the records stays with the device.
    void carryTillRecordsAreSent(Session& session, proto::router::SyncDataType type)
    {
        for (;;)
        {
            const QByteArray buffer = session.device.nextSentMessage();
            ASSERT_FALSE(buffer.isEmpty()) << "The device sends no records";

            proto::router::SyncToRouter message;
            ASSERT_TRUE(parse(buffer, &message));

            if (message.has_record_request())
            {
                ASSERT_GT(message.record_request().record_size(), 0);
                ASSERT_EQ(message.record_request().record(0).type(), type);
                return;
            }

            ASSERT_TRUE(carryRequest(session));
        }
    }

    // Carries the requests of the device till the device is about to ask for the records of |type|.
    // The request stays with the device.
    void carryTillRecordsAreAsked(Session& session, proto::router::SyncDataType type)
    {
        for (;;)
        {
            const QByteArray buffer = session.device.nextSentMessage();
            ASSERT_FALSE(buffer.isEmpty()) << "The device asks for no records";

            proto::router::SyncToRouter message;
            ASSERT_TRUE(parse(buffer, &message));

            if (message.has_record_list_request() && message.record_list_request().type() == type)
                return;

            ASSERT_TRUE(carryRequest(session));
        }
    }

    // Carries the messages between the devices and the router until nothing is left to carry.
    void settle()
    {
        int carried_count = 0;

        bool carried = true;
        while (carried)
        {
            carried = false;

            for (Session* session : { laptop_.get(), phone_.get() })
            {
                if (!carryRequest(*session))
                {
                    if (session->notifications.isEmpty())
                        continue;

                    session->device.receive(session->notifications.takeFirst());
                }

                carried = true;
                ASSERT_LT(++carried_count, 1000) << "The exchange does not end";
            }
        }
    }

    QTemporaryDir dir_;
    std::unique_ptr<Session> laptop_;
    std::unique_ptr<Session> phone_;
};

// Runs a scenario under every schedule that leaves the unhurried one in a few places. A scenario is
// what the user does on the two devices. A schedule is the order in which the actions of the user,
// the requests of the devices, the notifications of the router and a break of the session of the
// laptop take place; the unhurried one lets the devices and the router finish before the user acts
// again. After every schedule the devices have to hold the same records, with nothing left to send
// and with no error told to the user. After every step the tree of groups of a device has to be
// whole.
//
// Where it is known what the user left last, the records also have to be those. When only the laptop
// acts, it is what the laptop held right after its last action. When both devices act, and one of
// them took its last action holding exactly what the other one held after its own last action, it is
// what that device held right after: its user saw all the other one did and changed it. Otherwise the
// last actions were taken apart, and either of them may give way.
//
// A scenario may name the device that removes a group while the other one changes or adds what lies
// in that group. Such a removal wins under every schedule, and the records have to be what that
// device held right after it.
//
// A scenario may also ask for more deviations than the stand takes, when the order it is written
// for lies that far from the unhurried one.
//
// The devices stay in their sessions from one schedule to the next, as joining the storage takes
// long. Before a schedule the records of both devices are removed. Then both get a credential with a
// router host linking it, and the groups "office" and "home" with the group "branch" in the office
// and a local host in the branch that links the credential too.
class SyncExchangeScheduleTest : public SyncExchangeTest
{
protected:
    using Action = std::function<void()>;

    enum class Removal
    {
        NONE,
        ON_LAPTOP,
        ON_PHONE
    };

    struct Scenario
    {
        QList<Action> laptop_actions;
        QList<Action> phone_actions;

        // The removal of a group that wins over what the other device does in that group.
        Removal removal = Removal::NONE;

        // How many deviations the scenario asks for. They are taken when the stand takes fewer.
        int deviations = 0;
    };

    // In the order the unhurried schedule takes them.
    enum class Step
    {
        LAPTOP_REQUEST,
        PHONE_REQUEST,
        LAPTOP_NOTIFICATION,
        PHONE_NOTIFICATION,
        LAPTOP_SESSION,
        LAPTOP_ACTION,
        PHONE_ACTION,
        LAPTOP_ANSWER_LOST,
        LAPTOP_REQUEST_LOST
    };

    void SetUp() override
    {
        SyncExchangeTest::SetUp();

        bool ok = false;

        const int deviations = qEnvironmentVariableIntValue("ASPIA_SYNC_SCHEDULE_DEVIATIONS", &ok);
        if (ok)
            max_deviations_ = deviations;

        const int breaks = qEnvironmentVariableIntValue("ASPIA_SYNC_SCHEDULE_BREAKS", &ok);
        if (ok)
            max_breaks_ = breaks;

        laptop_->device.startSession();
        settle();

        phone_->device.startSession();
        settle();
    }

    // The actions of the user. They run in the thread of a device and do nothing when the record
    // they are about is not there any more.
    void editCredential(const QString& username) const
    {
        CredentialConfig credential;
        if (CredentialStore().findByGuid(credential_guid_, &credential) != Database::FindResult::FOUND)
            return;

        credential.setUsername(username);
        EXPECT_TRUE(CredentialStore().modify(credential));
    }

    void removeCredential() const
    {
        CredentialConfig credential;
        if (CredentialStore().findByGuid(credential_guid_, &credential) != Database::FindResult::FOUND)
            return;

        EXPECT_TRUE(CredentialStore().remove(credential.id()));
    }

    void editHost(const QString& username) const
    {
        RouterHostConfig host;
        if (RouterHostStore().find(kHostId, &host) != Database::FindResult::FOUND)
            return;

        host.setUsername(username);
        host.setPassword(SecureString(QString("secret")));
        EXPECT_TRUE(RouterHostStore().modify(host));
    }

    void removeHost() const
    {
        EXPECT_TRUE(RouterHostStore().remove(kHostId));
    }

    void addHost(const QString& username) const
    {
        RouterHostConfig host;
        if (RouterHostStore().find(kHostId, &host) == Database::FindResult::FOUND)
        {
            editHost(username);
            return;
        }

        host.setHostId(kHostId);
        host.setUsername(username);
        host.setPassword(SecureString(QString("secret")));
        EXPECT_TRUE(RouterHostStore().add(host));
    }

    void unlinkHost() const
    {
        RouterHostConfig host;
        if (RouterHostStore().find(kHostId, &host) != Database::FindResult::FOUND)
            return;

        host.setCredentialId(0);
        EXPECT_TRUE(RouterHostStore().modify(host));
    }

    void addCredentialAndLinkHost(const QString& name) const
    {
        CredentialConfig credential;
        credential.setDisplayName(name);
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        EXPECT_TRUE(CredentialStore().add(credential));

        RouterHostConfig host;
        if (RouterHostStore().find(kHostId, &host) != Database::FindResult::FOUND)
            return;

        host.setCredentialId(credential.id());
        EXPECT_TRUE(RouterHostStore().modify(host));
    }

    void addCredential(const QString& name)
    {
        CredentialConfig credential;
        credential.setDisplayName(name);
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        EXPECT_TRUE(CredentialStore().add(credential));

        CredentialConfig added;
        EXPECT_EQ(CredentialStore().find(credential.id(), &added), Database::FindResult::FOUND);
        added_credential_guid_ = added.guid();
    }

    void removeAddedCredential() const
    {
        CredentialConfig credential;
        if (CredentialStore().findByGuid(added_credential_guid_, &credential) != Database::FindResult::FOUND)
            return;

        EXPECT_TRUE(CredentialStore().remove(credential.id()));
    }

    static bool findGroup(const QString& guid, LocalGroupConfig* group)
    {
        qint64 group_id = 0;
        return LocalGroupStore().findRowIdByGuid(guid, &group_id) == Database::FindResult::FOUND &&
               LocalGroupStore().find(group_id, group) == Database::FindResult::FOUND;
    }

    static bool findLocalHost(const QString& guid, LocalHostConfig* host)
    {
        QList<LocalHostConfig> local_hosts;
        if (LocalHostStore().all(LocalHostStore::Scope::VISIBLE, &local_hosts) != Database::ReadResult::OK)
            return false;

        for (const LocalHostConfig& local_host : std::as_const(local_hosts))
        {
            if (local_host.guid() == guid)
            {
                *host = local_host;
                return true;
            }
        }

        return false;
    }

    // An empty |parent_guid| is the root.
    void addGroup(const QString& name, const QString& parent_guid)
    {
        LocalGroupConfig parent;
        if (!parent_guid.isEmpty() && !findGroup(parent_guid, &parent))
            return;

        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_guid.isEmpty() ? 0 : parent.id());
        EXPECT_TRUE(LocalGroupStore().add(group));

        added_group_guid_ = group.guid();
    }

    void renameGroup(const QString& guid, const QString& name) const
    {
        LocalGroupConfig group;
        if (!findGroup(guid, &group))
            return;

        group.setName(name);
        EXPECT_TRUE(LocalGroupStore().modify(group));
    }

    // A move that would close a loop is refused by the store, and the tree does not offer it.
    void moveGroup(const QString& guid, const QString& parent_guid) const
    {
        LocalGroupConfig group;
        LocalGroupConfig parent;
        if (!findGroup(guid, &group) || !findGroup(parent_guid, &parent))
            return;

        LocalGroupStore().move(group.id(), parent.id());
    }

    void moveGroupIntoNewGroup(const QString& guid, const QString& new_group_name)
    {
        addGroup(new_group_name, QString());
        moveGroup(guid, added_group_guid_);
    }

    void removeGroup(const QString& guid) const
    {
        LocalGroupConfig group;
        if (!findGroup(guid, &group))
            return;

        EXPECT_TRUE(LocalGroupStore().remove(group.id()));
    }

    void addLocalHost(const QString& name, const QString& group_guid) const
    {
        LocalGroupConfig group;
        if (!findGroup(group_guid, &group))
            return;

        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.2");
        host.setGroupId(group.id());
        EXPECT_TRUE(LocalHostStore().add(host));
    }

    void renameLocalHost(const QString& name) const
    {
        LocalHostConfig host;
        if (!findLocalHost(local_host_guid_, &host))
            return;

        host.setName(name);
        EXPECT_TRUE(LocalHostStore().modify(host));
    }

    void moveLocalHost(const QString& group_guid) const
    {
        LocalHostConfig host;
        LocalGroupConfig group;
        if (!findLocalHost(local_host_guid_, &host) || !findGroup(group_guid, &group))
            return;

        EXPECT_TRUE(LocalHostStore().move(host.id(), group.id()));
    }

    void removeLocalHost() const
    {
        LocalHostConfig host;
        if (!findLocalHost(local_host_guid_, &host))
            return;

        EXPECT_TRUE(LocalHostStore().remove(host.id()));
    }

    static void removeRecords()
    {
        QList<CredentialConfig> credentials;
        ASSERT_EQ(CredentialStore().list(&credentials), Database::ReadResult::OK);

        for (const CredentialConfig& credential : std::as_const(credentials))
            ASSERT_TRUE(CredentialStore().remove(credential.id()));

        QList<ProfileConfig> profiles;
        ASSERT_EQ(ProfileStore().list(&profiles), Database::ReadResult::OK);

        for (const ProfileConfig& profile : std::as_const(profiles))
        {
            if (!profile.isDefault())
                ASSERT_TRUE(ProfileStore().remove(profile.id()));
        }

        QList<RouterHostConfig> hosts;
        ASSERT_EQ(RouterHostStore().list(&hosts), Database::ReadResult::OK);

        for (const RouterHostConfig& host : std::as_const(hosts))
            ASSERT_TRUE(RouterHostStore().remove(host.hostId()));

        QList<LocalHostConfig> local_hosts;
        ASSERT_EQ(LocalHostStore().all(LocalHostStore::Scope::VISIBLE, &local_hosts), Database::ReadResult::OK);

        for (const LocalHostConfig& local_host : std::as_const(local_hosts))
            ASSERT_TRUE(LocalHostStore().remove(local_host.id()));

        QList<LocalGroupConfig> root_groups;
        ASSERT_TRUE(LocalGroupStore().list(0, &root_groups));

        for (const LocalGroupConfig& root_group : std::as_const(root_groups))
            ASSERT_TRUE(LocalGroupStore().remove(root_group.id()));
    }

    // What the device holds, and how many records it has to send.
    static void readRecords(QStringList* records, qsizetype* unsynced_count)
    {
        const auto credentialGuid = [](qint64 credential_id)
        {
            if (credential_id <= 0)
                return QString("none");

            CredentialConfig credential;
            if (CredentialStore().find(credential_id, &credential) != Database::FindResult::FOUND)
                return QString("none");

            return credential.guid();
        };

        QList<CredentialConfig> credentials;
        ASSERT_EQ(CredentialStore().list(&credentials), Database::ReadResult::OK);

        for (const CredentialConfig& credential : std::as_const(credentials))
        {
            records->append(QString("credential %1 %2 %3 %4").arg(credential.guid(), credential.displayName(),
                credential.username(), credential.password().toString()));
        }

        QList<ProfileConfig> profiles;
        ASSERT_EQ(ProfileStore().list(&profiles), Database::ReadResult::OK);

        for (const ProfileConfig& profile : std::as_const(profiles))
            records->append(QString("profile %1 %2").arg(profile.guid(), profile.name()));

        QList<RouterHostConfig> hosts;
        ASSERT_EQ(RouterHostStore().list(&hosts), Database::ReadResult::OK);

        for (const RouterHostConfig& host : std::as_const(hosts))
        {
            ProfileConfig profile;
            const bool has_profile = host.profileId() > 0 &&
                ProfileStore().find(host.profileId(), &profile) == Database::FindResult::FOUND;

            records->append(QString("host %1 %2 %3 credential %4 os credential %5 profile %6")
                .arg(host.hostId()).arg(host.username(), host.password().toString(),
                     credentialGuid(host.credentialId()), credentialGuid(host.osCredentialId()),
                     has_profile ? profile.guid() : QString("none")));
        }

        QList<LocalGroupConfig> groups;
        ASSERT_TRUE(LocalGroupStore().all(&groups));

        std::unordered_map<qint64, QString> group_guids;
        for (const LocalGroupConfig& group : std::as_const(groups))
            group_guids.emplace(group.id(), group.guid());

        const auto groupGuid = [&group_guids](qint64 group_id)
        {
            const auto group_guid = group_guids.find(group_id);
            return group_guid == group_guids.end() ? QString("root") : group_guid->second;
        };

        for (const LocalGroupConfig& group : std::as_const(groups))
            records->append(QString("group %1 %2 in %3").arg(group.guid(), group.name(), groupGuid(group.parentId())));

        QList<LocalHostConfig> local_hosts;
        ASSERT_EQ(LocalHostStore().all(LocalHostStore::Scope::VISIBLE, &local_hosts), Database::ReadResult::OK);

        for (const LocalHostConfig& local_host : std::as_const(local_hosts))
        {
            records->append(QString("local host %1 %2 %3 in %4 credential %5").arg(local_host.guid(),
                local_host.name(), local_host.address(), groupGuid(local_host.groupId()),
                credentialGuid(local_host.credentialId())));
        }

        records->sort();

        QList<CredentialStore::Unsynced> unsynced_credentials;
        ASSERT_EQ(CredentialStore().unsynced(&unsynced_credentials), Database::ReadResult::OK);

        QList<ProfileStore::Unsynced> unsynced_profiles;
        ASSERT_EQ(ProfileStore().unsynced(&unsynced_profiles), Database::ReadResult::OK);

        QList<RouterHostStore::Unsynced> unsynced_hosts;
        ASSERT_EQ(RouterHostStore().unsynced(&unsynced_hosts), Database::ReadResult::OK);

        QList<LocalGroupStore::Unsynced> unsynced_groups;
        ASSERT_EQ(LocalGroupStore().unsynced(&unsynced_groups), Database::ReadResult::OK);

        QList<LocalHostStore::Unsynced> unsynced_local_hosts;
        ASSERT_EQ(LocalHostStore().unsynced(&unsynced_local_hosts), Database::ReadResult::OK);

        *unsynced_count = unsynced_credentials.size() + unsynced_profiles.size() + unsynced_hosts.size() +
            unsynced_groups.size() + unsynced_local_hosts.size();
    }

    // The groups and the local hosts of the device |name| that wait for a group. The stores do not
    // hand them out, so they are counted in the file of the device.
    qint64 waitingRowCount(const QString& name) const
    {
        SqlDatabase raw;
        if (!raw.open(dir_.filePath(name + ".db3")))
            return -1;

        qint64 count = 0;

        for (const char* sql : { "SELECT COUNT(*) FROM local_groups WHERE sync_state=?",
                                 "SELECT COUNT(*) FROM local_hosts WHERE sync_state=?" })
        {
            SqlQuery query(raw, sql);
            query.addInt64(SyncStore::SYNC_STATE_WAITING);

            if (query.next() != SqlQuery::StepResult::ROW)
                return -1;

            count += query.columnInt64(0);
        }

        return count;
    }

    // Every group the device shows lies in the root or in a group it shows, and so does every local
    // host. A group that lies in a loop never reaches the root.
    static void expectTreeIsWhole(const char* device_name)
    {
        QList<LocalGroupConfig> groups;
        ASSERT_TRUE(LocalGroupStore().all(&groups));

        std::unordered_map<qint64, qint64> parent_ids;
        for (const LocalGroupConfig& group : std::as_const(groups))
            parent_ids.emplace(group.id(), group.parentId());

        for (const LocalGroupConfig& group : std::as_const(groups))
        {
            qint64 group_id = group.id();

            for (size_t passed_count = 0; group_id > 0; ++passed_count)
            {
                const auto parent_id = parent_ids.find(group_id);

                ASSERT_TRUE(parent_id != parent_ids.end())
                    << "Group " << group.name().toStdString() << " of the " << device_name
                    << " lies below a group that is not shown";
                ASSERT_LT(passed_count, parent_ids.size())
                    << "Group " << group.name().toStdString() << " of the " << device_name << " lies in a loop";

                group_id = parent_id->second;
            }
        }

        QList<LocalHostConfig> local_hosts;
        ASSERT_EQ(LocalHostStore().all(LocalHostStore::Scope::VISIBLE, &local_hosts), Database::ReadResult::OK);

        for (const LocalHostConfig& local_host : std::as_const(local_hosts))
        {
            EXPECT_TRUE(local_host.groupId() <= 0 || parent_ids.find(local_host.groupId()) != parent_ids.end())
                << "Local host " << local_host.name().toStdString() << " of the " << device_name
                << " lies in a group that is not shown";
        }
    }

    void expectTreesAreWhole()
    {
        laptop_->device.run([]() { expectTreeIsWhole("laptop"); });
        phone_->device.run([]() { expectTreeIsWhole("phone"); });
    }

    static QStringList heldRecords(Session& session)
    {
        QStringList records;

        session.device.run([&]()
        {
            qsizetype unsynced_count = 0;
            readRecords(&records, &unsynced_count);
        });

        return records;
    }

    // Lets both devices exchange once more, the way the timer of the worker does, and compares them.
    // |expected_records| is not empty when it is known what the devices have to hold.
    void expectDevicesMatch(const QStringList& expected_records)
    {
        laptop_->device.startExchange();
        phone_->device.startExchange();
        ASSERT_NO_FATAL_FAILURE(settle());

        QStringList laptop_records;
        QStringList phone_records;
        qsizetype laptop_unsynced_count = 0;
        qsizetype phone_unsynced_count = 0;

        laptop_->device.run([&]() { readRecords(&laptop_records, &laptop_unsynced_count); });
        phone_->device.run([&]() { readRecords(&phone_records, &phone_unsynced_count); });

        EXPECT_EQ(laptop_records.join('\n').toStdString(), phone_records.join('\n').toStdString());

        if (!expected_records.isEmpty())
        {
            EXPECT_EQ(laptop_records.join('\n').toStdString(), expected_records.join('\n').toStdString())
                << "The devices do not hold what the user left last";
        }

        EXPECT_EQ(laptop_unsynced_count, 0);
        EXPECT_EQ(phone_unsynced_count, 0);

        EXPECT_EQ(waitingRowCount("laptop"), 0);
        EXPECT_EQ(waitingRowCount("phone"), 0);

        expectTreesAreWhole();

        for (Session* session : { laptop_.get(), phone_.get() })
        {
            for (SyncWorker::Event event : session->device.events())
                EXPECT_EQ(event, SyncWorker::Event::STORAGE_ATTACHED);
        }
    }

    void prepare()
    {
        for (Session* session : { laptop_.get(), phone_.get() })
        {
            session->device.run([]() { removeRecords(); });
            session->device.startExchange();
        }

        ASSERT_NO_FATAL_FAILURE(settle());

        laptop_->device.run([&]()
        {
            CredentialConfig credential;
            credential.setDisplayName("office");
            credential.setUsername("admin");
            credential.setPassword(SecureString(QString("password")));
            ASSERT_TRUE(CredentialStore().add(credential));
            ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
            credential_guid_ = credential.guid();

            RouterHostConfig host;
            host.setHostId(kHostId);
            host.setUsername("user");
            host.setPassword(SecureString(QString("secret")));
            host.setCredentialId(credential.id());
            ASSERT_TRUE(RouterHostStore().add(host));

            LocalGroupConfig office;
            office.setName("office");
            ASSERT_TRUE(LocalGroupStore().add(office));
            office_guid_ = office.guid();

            LocalGroupConfig branch;
            branch.setName("branch");
            branch.setParentId(office.id());
            ASSERT_TRUE(LocalGroupStore().add(branch));
            branch_guid_ = branch.guid();

            LocalGroupConfig home;
            home.setName("home");
            ASSERT_TRUE(LocalGroupStore().add(home));
            home_guid_ = home.guid();

            LocalHostConfig local_host;
            local_host.setName("desk");
            local_host.setAddress("192.168.0.1");
            local_host.setGroupId(branch.id());
            local_host.setCredentialId(credential.id());
            ASSERT_TRUE(LocalHostStore().add(local_host));
            local_host_guid_ = local_host.guid();
        });

        laptop_->device.startExchange();
        ASSERT_NO_FATAL_FAILURE(settle());
    }

    // Takes at every step the enabled step number |schedule[index]|, the first one past the end of
    // |schedule|. |enabled_counts| gets how many steps every step was chosen from. |expected_records|
    // gets what the user left last, and stays empty when that is not known.
    void runSchedule(const Scenario& scenario, const QList<int>& schedule, QList<int>* enabled_counts,
        QStringList* taken_steps, QStringList* expected_records)
    {
        qsizetype laptop_action = 0;
        qsizetype phone_action = 0;
        bool laptop_online = true;
        int break_count = 0;

        // What a device held right before and right after the last action taken on it.
        QStringList laptop_before;
        QStringList laptop_after;
        QStringList phone_before;
        QStringList phone_after;

        for (int index = 0;; ++index)
        {
            ASSERT_LT(index, 300) << "The scenario does not end";

            const bool laptop_request = laptop_online && !laptop_->device.nextSentMessage().isEmpty();

            QList<Step> enabled;

            if (laptop_request)
                enabled.append(Step::LAPTOP_REQUEST);
            if (!phone_->device.nextSentMessage().isEmpty())
                enabled.append(Step::PHONE_REQUEST);
            if (!laptop_->notifications.isEmpty())
                enabled.append(Step::LAPTOP_NOTIFICATION);
            if (!phone_->notifications.isEmpty())
                enabled.append(Step::PHONE_NOTIFICATION);
            if (!laptop_online)
                enabled.append(Step::LAPTOP_SESSION);
            if (laptop_action < scenario.laptop_actions.size())
                enabled.append(Step::LAPTOP_ACTION);
            if (phone_action < scenario.phone_actions.size())
                enabled.append(Step::PHONE_ACTION);

            if (laptop_request && break_count < max_breaks_)
            {
                enabled.append(Step::LAPTOP_ANSWER_LOST);
                enabled.append(Step::LAPTOP_REQUEST_LOST);
            }

            if (enabled.isEmpty())
                break;

            const int choice = schedule.value(index, 0);
            ASSERT_LT(choice, enabled.size()) << "The schedule does not repeat";

            enabled_counts->append(enabled.size());

            switch (enabled[choice])
            {
                case Step::LAPTOP_REQUEST:
                    taken_steps->append("laptop request");
                    ASSERT_TRUE(carryRequest(*laptop_));
                    break;

                case Step::PHONE_REQUEST:
                    taken_steps->append("phone request");
                    ASSERT_TRUE(carryRequest(*phone_));
                    break;

                case Step::LAPTOP_NOTIFICATION:
                    taken_steps->append("laptop notification");
                    laptop_->device.receive(laptop_->notifications.takeFirst());
                    break;

                case Step::PHONE_NOTIFICATION:
                    taken_steps->append("phone notification");
                    phone_->device.receive(phone_->notifications.takeFirst());
                    break;

                case Step::LAPTOP_SESSION:
                    taken_steps->append("laptop session");
                    laptop_->device.startSession();
                    laptop_online = true;
                    break;

                case Step::LAPTOP_ACTION:
                    taken_steps->append(QString("laptop action %1").arg(laptop_action + 1));
                    laptop_before = heldRecords(*laptop_);
                    laptop_->device.run(scenario.laptop_actions[laptop_action++]);
                    laptop_after = heldRecords(*laptop_);
                    laptop_->device.startExchange();
                    break;

                case Step::PHONE_ACTION:
                    taken_steps->append(QString("phone action %1").arg(phone_action + 1));
                    phone_before = heldRecords(*phone_);
                    phone_->device.run(scenario.phone_actions[phone_action++]);
                    phone_after = heldRecords(*phone_);
                    phone_->device.startExchange();
                    break;

                case Step::LAPTOP_ANSWER_LOST:
                    taken_steps->append("laptop answer lost");
                    ASSERT_NO_FATAL_FAILURE(breakSessionAtRequest(*laptop_));
                    laptop_online = false;
                    ++break_count;
                    break;

                case Step::LAPTOP_REQUEST_LOST:
                    taken_steps->append("laptop request lost");
                    ASSERT_NO_FATAL_FAILURE(breakSessionBeforeRequest(*laptop_));
                    laptop_online = false;
                    ++break_count;
                    break;
            }

            // A record waits for its group only between the steps, so the end does not show it.
            expectTreesAreWhole();

            if (HasFailure())
                return;
        }

        if (scenario.phone_actions.isEmpty() || scenario.removal == Removal::ON_LAPTOP)
        {
            *expected_records = laptop_after;
            return;
        }

        if (scenario.removal == Removal::ON_PHONE)
        {
            *expected_records = phone_after;
            return;
        }

        const bool phone_saw_laptop = phone_before == laptop_after;
        const bool laptop_saw_phone = laptop_before == phone_after;

        if (phone_saw_laptop && !laptop_saw_phone)
            *expected_records = phone_after;
        else if (laptop_saw_phone && !phone_saw_laptop)
            *expected_records = laptop_after;
    }

    void runEverySchedule(const Scenario& scenario)
    {
        QElapsedTimer timer;
        timer.start();

        const int max_deviations = std::max(max_deviations_, scenario.deviations);

        QList<int> schedule;
        int schedule_count = 0;
        int known_outcome_count = 0;

        for (;;)
        {
            QList<int> enabled_counts;
            QStringList taken_steps;
            QStringList expected_records;

            prepare();

            if (!HasFailure())
                runSchedule(scenario, schedule, &enabled_counts, &taken_steps, &expected_records);

            if (!HasFailure())
                expectDevicesMatch(expected_records);

            ++schedule_count;

            if (!expected_records.isEmpty())
                ++known_outcome_count;

            if (HasFailure())
            {
                ADD_FAILURE() << "Schedule " << schedule_count << ": " << taken_steps.join(", ").toStdString();
                return;
            }

            qsizetype index = enabled_counts.size() - 1;

            for (; index >= 0; --index)
            {
                int deviation_count = 0;

                for (qsizetype i = 0; i < index; ++i)
                {
                    if (schedule.value(i, 0) != 0)
                        ++deviation_count;
                }

                if (schedule.value(index, 0) + 1 < enabled_counts[index] && deviation_count < max_deviations)
                    break;
            }

            if (index < 0)
                break;

            const int next_choice = schedule.value(index, 0) + 1;

            while (schedule.size() < index)
                schedule.append(0);

            schedule.resize(index);
            schedule.append(next_choice);
        }

        std::cout << "[  INFO ] Schedules: " << schedule_count << ", with a known outcome: " << known_outcome_count
                  << ", deviations: " << max_deviations << ", breaks: " << max_breaks_ << ", ms: "
                  << timer.elapsed() << std::endl;
    }

    int max_deviations_ = 1;
    int max_breaks_ = 0;
    QString credential_guid_;
    QString added_credential_guid_;
    QString office_guid_;
    QString branch_guid_;
    QString home_guid_;
    QString added_group_guid_;
    QString local_host_guid_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A host with its own pair and its links, made on the laptop, reaches the phone through the router:
// the phone has the pair, and the links point there at the records of the phone.
TEST_F(SyncExchangeTest, HostWithItsLinksReachesTheOtherDevice)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;
    QString profile_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        ProfileConfig profile;
        profile.setName("office");
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);
        ASSERT_TRUE(ProfileStore().add(profile));
        ASSERT_EQ(ProfileStore().find(profile.id(), &profile), Database::FindResult::FOUND);
        profile_guid = profile.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        host.setProfileId(profile.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        EXPECT_EQ(host.username(), QString("user"));
        EXPECT_EQ(host.password().toString(), QString("secret"));

        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().find(host.credentialId(), &credential), Database::FindResult::FOUND);
        EXPECT_EQ(credential.guid(), credential_guid);

        ProfileConfig profile;
        ASSERT_EQ(ProfileStore().find(host.profileId(), &profile), Database::FindResult::FOUND);
        EXPECT_EQ(profile.guid(), profile_guid);
    });

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED });

        session->device.run([]()
        {
            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK);
            EXPECT_TRUE(hosts.isEmpty());
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A group with a group and a host in it, made on the laptop, reaches the phone through the router
// with its tree.
TEST_F(SyncExchangeTest, GroupsAndHostsOfTheDeviceReachTheOtherDevice)
{
    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        LocalGroupConfig office;
        office.setName("office");
        ASSERT_TRUE(LocalGroupStore().add(office));

        LocalGroupConfig branch;
        branch.setName("branch");
        branch.setParentId(office.id());
        ASSERT_TRUE(LocalGroupStore().add(branch));

        LocalHostConfig host;
        host.setName("host");
        host.setAddress("192.168.0.1");
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setGroupId(branch.id());
        ASSERT_TRUE(LocalHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        QList<LocalGroupConfig> root_groups;
        ASSERT_TRUE(LocalGroupStore().list(0, &root_groups));
        ASSERT_EQ(root_groups.size(), 1);
        EXPECT_EQ(root_groups[0].name(), QString("office"));

        QList<LocalGroupConfig> child_groups;
        ASSERT_TRUE(LocalGroupStore().list(root_groups[0].id(), &child_groups));
        ASSERT_EQ(child_groups.size(), 1);
        EXPECT_EQ(child_groups[0].name(), QString("branch"));

        QList<LocalHostConfig> hosts;
        ASSERT_EQ(LocalHostStore().list(child_groups[0].id(), &hosts), Database::ReadResult::OK);
        ASSERT_EQ(hosts.size(), 1);
        EXPECT_EQ(hosts[0].name(), QString("host"));
        EXPECT_EQ(hosts[0].address(), QString("192.168.0.1"));
        EXPECT_EQ(hosts[0].username(), QString("user"));
        EXPECT_EQ(hosts[0].password().toString(), QString("secret"));
    });

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED });

        session->device.run([]()
        {
            QList<LocalGroupStore::Unsynced> groups;
            EXPECT_EQ(LocalGroupStore().unsynced(&groups), Database::ReadResult::OK);
            EXPECT_TRUE(groups.isEmpty());

            QList<LocalHostStore::Unsynced> hosts;
            EXPECT_EQ(LocalHostStore().unsynced(&hosts), Database::ReadResult::OK);
            EXPECT_TRUE(hosts.isEmpty());
        });
    }
}

//--------------------------------------------------------------------------------------------------
// The phone has asked for the credentials and has not asked for the hosts yet, when the laptop sends
// a credential and a host linking it. The phone gets the host first and the credential with the next
// exchange: the host has to link the credential then.
TEST_F(SyncExchangeTest, HostThatCameBeforeItsCredentialLinksItOnceItComes)
{
    laptop_->device.startSession();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_list_request().type(), proto::router::SYNC_DATA_TYPE_PROFILES);

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        EXPECT_EQ(host.credentialId(), credential.id());
    });
}

//--------------------------------------------------------------------------------------------------
// The credential a host links is removed on the phone, and the removal gives way: the laptop changes
// the credential right before the router gets the removal. A change of the host, made on the laptop
// earlier, comes to the phone while the credential is still marked as removed there. The credential
// comes back, and the host has to link it as it does on the laptop.
TEST_F(SyncExchangeTest, HostKeepsTheCredentialWhoseRemovalGaveWay)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setUsername("another");
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        ASSERT_TRUE(CredentialStore().remove(credential.id()));
    });

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_request().record_size(), 1);
    ASSERT_EQ(phone_message.record_request().record(0).type(), proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    ASSERT_TRUE(phone_message.record_request().record(0).deleted());

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername("root");
        ASSERT_TRUE(CredentialStore().modify(credential));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        EXPECT_EQ(credential.username(), QString("root"));

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        EXPECT_EQ(host.username(), QString("another"));
        EXPECT_EQ(host.credentialId(), credential.id());
    });
}

//--------------------------------------------------------------------------------------------------
// The credential a host links is removed on the phone, and the host is changed there before the
// exchange. The removal gives way: the laptop changes the credential right before the router gets
// the removal. The change of the host goes to the router while the credential is still marked as
// removed on the phone. The credential comes back, and the host has to link it on both devices.
TEST_F(SyncExchangeTest, ChangedHostKeepsTheCredentialWhoseRemovalGaveWay)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        ASSERT_TRUE(CredentialStore().remove(credential.id()));

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        ASSERT_EQ(host.credentialId(), credential.id());

        host.setUsername("another");
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_request().record_size(), 1);
    ASSERT_EQ(phone_message.record_request().record(0).type(), proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    ASSERT_TRUE(phone_message.record_request().record(0).deleted());

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername("root");
        ASSERT_TRUE(CredentialStore().modify(credential));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;
            EXPECT_EQ(credential.username(), QString("root")) << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username(), QString("another")) << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A host has no pair of its own, all it holds is the link to a credential. The credential is removed
// on the phone, the host is saved there once more, and the removal gives way. The host goes to the
// router with its link while the credential is still marked as removed on the phone: the exchange
// does not fail, and the host links the credential on both devices.
TEST_F(SyncExchangeTest, HostWithNothingButALinkKeepsTheCredentialWhoseRemovalGaveWay)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        ASSERT_TRUE(CredentialStore().remove(credential.id()));

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        ASSERT_EQ(host.credentialId(), credential.id());
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_request().record_size(), 1);
    ASSERT_EQ(phone_message.record_request().record(0).type(), proto::router::SYNC_DATA_TYPE_CREDENTIALS);
    ASSERT_TRUE(phone_message.record_request().record(0).deleted());

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername("root");
        ASSERT_TRUE(CredentialStore().modify(credential));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED })
            << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;
            EXPECT_EQ(credential.username(), QString("root")) << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A change of a credential reaches the router, and the session breaks before the laptop gets the
// answer. The credential is changed on the laptop once more before the next session. The later
// change is the one both devices have to end with.
TEST_F(SyncExchangeTest, ChangeMadeAfterALostAnswerIsKept)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().find(credential_id, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    const auto storedUsername = [&]()
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        return credential.username().toStdString();
    };

    laptop_->device.run([&]() { setUsername("first"); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(storedUsername(), "first"); });

    laptop_->device.run([&]() { setUsername("second"); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(storedUsername(), "second") << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(storedUsername(), "second") << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// A change of a credential reaches the router, and the session breaks before the laptop gets the
// answer. The credential is removed on the laptop before the next session. The removal is what both
// devices have to end with.
TEST_F(SyncExchangeTest, RemovalMadeAfterALostAnswerIsKept)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().find(credential_id, &credential), Database::FindResult::FOUND);

        credential.setUsername("first");
        ASSERT_TRUE(CredentialStore().modify(credential));
    });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findCredential(), Database::FindResult::FOUND); });

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// A new credential reaches the router, and the session breaks before the laptop gets the answer.
// The credential is removed on the laptop before the next session. The removal is what both devices
// have to end with.
TEST_F(SyncExchangeTest, NewRecordRemovedAfterALostAnswerStaysRemoved)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findCredential(), Database::FindResult::FOUND); });

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The credential a host links is removed, and the router still holds the host with the link. A
// device that comes to the storage later gets the host at once: the exchange ends, and the host is
// there with its own pair and links no credential.
TEST_F(SyncExchangeTest, HostOfARemovedCredentialComesLinkingNoCredential)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED })
            << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::NOT_FOUND)
                << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "user") << device_name;
            EXPECT_EQ(CredentialStore().find(host.credentialId(), &credential), Database::FindResult::NOT_FOUND)
                << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A host has no pair of its own, all it holds is the link to a credential. The phone has asked for
// the credentials and has not asked for the hosts yet, when the laptop sends the credential and the
// host. The phone gets the host first and the credential with the next exchange: the host has to be
// there and link the credential.
TEST_F(SyncExchangeTest, HostWithNothingButALinkThatCameBeforeItsCredentialLinksIt)
{
    laptop_->device.startSession();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_list_request().type(), proto::router::SYNC_DATA_TYPE_PROFILES);

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        EXPECT_EQ(host.credentialId(), credential.id());
    });
}

//--------------------------------------------------------------------------------------------------
// The phone has not taken a newer version of a host yet, because the credential the host links has
// not come, and the host is changed on the phone meanwhile. The change of the phone gives way to the
// version of the router, and both devices end with the same host.
TEST_F(SyncExchangeTest, ChangeOfAHostThatWaitsForItsCredentialGivesWay)
{
    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.startExchange();
    ASSERT_TRUE(carryRequest(*phone_));

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    proto::router::SyncToRouter phone_message;
    ASSERT_TRUE(parse(phone_request, &phone_message));
    ASSERT_EQ(phone_message.record_list_request().type(), proto::router::SYNC_DATA_TYPE_PROFILES);

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setUsername("laptop");
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    phone_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setUsername("phone");
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    readSyncMessage(*phone_, phone_request);
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "laptop") << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A change of a credential reaches the router, and the session breaks before the laptop gets the
// answer. The phone changes the credential after that, and the laptop changes it once more before
// its next session. The change of the phone is the one the router holds, and both devices end with it.
TEST_F(SyncExchangeTest, ChangeMadeAfterALostAnswerGivesWayToAChangeOfAnotherDevice)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    const auto storedUsername = [&]()
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        return credential.username().toStdString();
    };

    laptop_->device.run([&]() { setUsername("first"); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);
    settle();

    phone_->device.run([&]()
    {
        ASSERT_EQ(storedUsername(), "first");
        setUsername("phone");
    });

    phone_->device.startExchange();
    settle();

    laptop_->device.run([&]() { setUsername("second"); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// A new host reaches the router, and the session breaks before the laptop gets the answer. The host
// is removed on the laptop before the next session. The removal is what both devices have to end
// with.
TEST_F(SyncExchangeTest, NewHostRemovedAfterALostAnswerStaysRemoved)
{
    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    const auto findHost = [&]()
    {
        RouterHostConfig host;
        return RouterHostStore().find(kHostId, &host);
    };

    laptop_->device.startExchange();
    carryTillRecordsAreSent(*laptop_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);
    breakSessionAtRequest(*laptop_);

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findHost(), Database::FindResult::FOUND); });

    laptop_->device.run([&]() { ASSERT_TRUE(RouterHostStore().remove(kHostId)); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(findHost(), Database::FindResult::NOT_FOUND) << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(findHost(), Database::FindResult::NOT_FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The pair of the same host is entered on both devices before either of them tells the router. The
// laptop reaches the router first, the phone sends its host right after and is refused. Both devices
// end with the host of the laptop and with nothing left to send.
TEST_F(SyncExchangeTest, HostEnteredOnBothDevicesEndsTheSame)
{
    laptop_->device.startSession();
    settle();

    phone_->device.startSession();
    settle();

    const auto addHost = [&](const QString& username)
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername(username);
        host.setPassword(SecureString(QString("secret")));
        ASSERT_TRUE(RouterHostStore().add(host));
    };

    phone_->device.run([&]() { addHost("phone"); });

    phone_->device.startExchange();
    carryTillRecordsAreSent(*phone_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);

    laptop_->device.run([&]() { addHost("laptop"); });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED })
            << device_name;

        session->device.run([&]()
        {
            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "laptop") << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// The synchronization is turned off on the laptop and on again later. Meanwhile a credential is
// changed on both devices, and the laptop gets a new one. The device joins the storage anew: the
// change the router holds stays, the new credential goes to the storage, the host keeps its link,
// and both devices end the same.
TEST_F(SyncExchangeTest, SyncTurnedOffAndOnEndsTheSameOnBothDevices)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;
    QString new_credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    laptop_->device.setSyncEnabled(false);
    settle();

    laptop_->device.run([&]()
    {
        setUsername("offline");

        CredentialConfig credential;
        credential.setDisplayName("home");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        new_credential_guid = credential.guid();
    });

    phone_->device.run([&]() { setUsername("phone"); });

    phone_->device.startExchange();
    settle();

    laptop_->device.setSyncEnabled(true);
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        for (SyncWorker::Event event : session->device.events())
            EXPECT_EQ(event, SyncWorker::Event::STORAGE_ATTACHED) << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;
            EXPECT_EQ(credential.username().toStdString(), "phone") << device_name;

            CredentialConfig new_credential;
            EXPECT_EQ(CredentialStore().findByGuid(new_credential_guid, &new_credential),
                      Database::FindResult::FOUND) << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "user") << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;

            QList<CredentialStore::Unsynced> credentials;
            EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(credentials.isEmpty()) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// The laptop resets the storage. The storage is filled with what the laptop holds. The phone drops
// what it holds, a credential it has not sent included, and takes the storage: both devices end with
// the credential and the host of the laptop, and the host links the credential on both.
TEST_F(SyncExchangeTest, StorageResetLeavesBothDevicesWithTheDataOfTheResetter)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;
    QString phone_credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("phone-only");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        phone_credential_guid = credential.guid();
    });

    laptop_->device.resetStorage();
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        for (SyncWorker::Event event : session->device.events())
            EXPECT_EQ(event, SyncWorker::Event::STORAGE_ATTACHED) << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;
            EXPECT_EQ(credential.username().toStdString(), "admin") << device_name;

            CredentialConfig phone_credential;
            EXPECT_EQ(CredentialStore().findByGuid(phone_credential_guid, &phone_credential),
                      Database::FindResult::NOT_FOUND) << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "user") << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;

            QList<CredentialStore::Unsynced> credentials;
            EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(credentials.isEmpty()) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// More records than a request or an answer carries: the laptop sends them in several requests, the
// phone reads them page by page, and every host links on the phone the credential it links on the
// laptop.
TEST_F(SyncExchangeTest, RecordsThatTakeSeveralPagesReachTheOtherDevice)
{
    static const int kCount = 150;

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        for (int i = 0; i < kCount; ++i)
        {
            CredentialConfig credential;
            credential.setDisplayName(QString("office %1").arg(i));
            credential.setUsername("admin");
            credential.setPassword(SecureString(QString("password")));
            ASSERT_TRUE(CredentialStore().add(credential));

            RouterHostConfig host;
            host.setHostId(kHostId + i);
            host.setCredentialId(credential.id());
            ASSERT_TRUE(RouterHostStore().add(host));
        }
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    // The guid of the credential every host links.
    const auto linkedCredentials = [&](QHash<HostId, QString>* links)
    {
        QList<RouterHostConfig> hosts;
        ASSERT_EQ(RouterHostStore().list(&hosts), Database::ReadResult::OK);

        for (const RouterHostConfig& host : std::as_const(hosts))
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().find(host.credentialId(), &credential), Database::FindResult::FOUND);
            links->insert(host.hostId(), credential.guid());
        }
    };

    QHash<HostId, QString> laptop_links;
    QHash<HostId, QString> phone_links;

    laptop_->device.run([&]() { linkedCredentials(&laptop_links); });
    phone_->device.run([&]() { linkedCredentials(&phone_links); });

    EXPECT_EQ(laptop_links.size(), kCount);
    EXPECT_EQ(phone_links.size(), kCount);
    EXPECT_TRUE(phone_links == laptop_links);

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED });
    }
}

//--------------------------------------------------------------------------------------------------
// The phone has asked for the credentials and the profiles and has not asked for the hosts yet, when
// the laptop sends a profile and a host linking it. The phone gets the host first and the profile
// with the next exchange: the host has to link the profile then.
TEST_F(SyncExchangeTest, HostThatCameBeforeItsProfileLinksItOnceItComes)
{
    laptop_->device.startSession();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.startExchange();
    carryTillRecordsAreAsked(*phone_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);

    QByteArray phone_request;
    ASSERT_TRUE(phone_->device.takeSentMessage(&phone_request));

    QString profile_guid;

    laptop_->device.run([&]()
    {
        ProfileConfig profile;
        profile.setName("office");
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);
        ASSERT_TRUE(ProfileStore().add(profile));
        ASSERT_EQ(ProfileStore().find(profile.id(), &profile), Database::FindResult::FOUND);
        profile_guid = profile.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setProfileId(profile.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    readSyncMessage(*phone_, phone_request);
    settle();

    phone_->device.run([&]()
    {
        ProfileConfig profile;
        ASSERT_EQ(ProfileStore().findByGuid(profile_guid, &profile), Database::FindResult::FOUND);

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        EXPECT_EQ(host.profileId(), profile.id());
    });
}

//--------------------------------------------------------------------------------------------------
// A credential is changed on the phone, and the phone has not sent the change yet. The
// synchronization is turned off and on again on the laptop, where nothing was changed. The change of
// the phone is the only one made, so both devices have to end with it.
TEST_F(SyncExchangeTest, SyncTurnedOffAndOnLeavesAnUnsentChangeOfAnotherDevice)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto storedUsername = [&]()
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        return credential.username().toStdString();
    };

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername("phone");
        ASSERT_TRUE(CredentialStore().modify(credential));
    });

    laptop_->device.setSyncEnabled(false);
    laptop_->device.setSyncEnabled(true);
    settle();

    phone_->device.startExchange();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// A new host is on its way to the router. Before the answer comes, the host is removed on the laptop
// and entered anew with another pair. The answer is not taken for the host entered anew: its pair
// goes to the router, and both devices end with it.
TEST_F(SyncExchangeTest, HostRemovedAndEnteredAnewBeforeTheAnswerKeepsItsNewPair)
{
    laptop_->device.startSession();
    settle();

    const auto addHost = [&](const QString& username)
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername(username);
        host.setPassword(SecureString(QString("secret")));
        ASSERT_TRUE(RouterHostStore().add(host));
    };

    laptop_->device.run([&]() { addHost("user"); });

    laptop_->device.startExchange();
    carryTillRecordsAreSent(*laptop_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);

    QByteArray laptop_request;
    ASSERT_TRUE(laptop_->device.takeSentMessage(&laptop_request));

    laptop_->device.run([&]()
    {
        ASSERT_TRUE(RouterHostStore().remove(kHostId));
        addHost("another");
    });

    readSyncMessage(*laptop_, laptop_request);
    settle();

    phone_->device.startSession();
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        session->device.run([&]()
        {
            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "another") << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A host has no pair of its own, all it holds is the link to a credential. The credential is
// removed on the laptop. The host has nothing left to hold, so it is gone from both devices together
// with the credential.
TEST_F(SyncExchangeTest, HostThatHeldNothingButARemovedCredentialIsGoneFromBothDevices)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
    });

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startExchange();
    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED })
            << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::NOT_FOUND)
                << device_name;

            RouterHostConfig host;
            EXPECT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::NOT_FOUND) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// The pair of a host is changed on the phone, and the credential the host links is removed on the
// laptop before the phone tells the router. Both devices end with the pair of the phone and without
// the credential and the link.
TEST_F(SyncExchangeTest, HostChangedOnOneDeviceWhileItsCredentialIsRemovedOnTheOther)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();

        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setUsername("phone");
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startExchange();
    while (carryRequest(*laptop_))
    {
        // Nothing
    }

    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        EXPECT_EQ(session->device.events(), QList<SyncWorker::Event>{ SyncWorker::Event::STORAGE_ATTACHED })
            << device_name;

        session->device.run([&]()
        {
            CredentialConfig credential;
            EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::NOT_FOUND)
                << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "phone") << device_name;
            EXPECT_EQ(host.credentialId(), 0) << device_name;

            QList<RouterHostStore::Unsynced> hosts;
            EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK) << device_name;
            EXPECT_TRUE(hosts.isEmpty()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// The session breaks before a new credential reaches the router. The credential goes to the router
// with the next session and comes to the phone.
TEST_F(SyncExchangeTest, NewRecordWhoseRequestWasLostIsSentWithTheNextSession)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    const auto unsyncedCredentialCount = [&]()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK);
        return credentials.size();
    };

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionBeforeRequest(*laptop_);

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findCredential(), Database::FindResult::NOT_FOUND); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        EXPECT_EQ(findCredential(), Database::FindResult::FOUND) << "laptop";
        EXPECT_EQ(unsyncedCredentialCount(), 0) << "laptop";
    });

    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The session breaks before a new credential reaches the router, and the credential is removed on
// the laptop before the next session. Neither device has it in the end, and the laptop has nothing
// left to send.
TEST_F(SyncExchangeTest, NewRecordRemovedAfterALostRequestIsGoneFromBothDevices)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    const auto unsyncedCredentialCount = [&]()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK);
        return credentials.size();
    };

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionBeforeRequest(*laptop_);

    phone_->device.startSession();
    settle();

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "laptop";
        EXPECT_EQ(unsyncedCredentialCount(), 0) << "laptop";
    });

    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The removal of a credential reaches the router, and the session breaks before the laptop gets the
// answer. With the next session the laptop learns that the router holds the removal: the credential
// is gone from both devices, and the laptop has nothing left to send.
TEST_F(SyncExchangeTest, RemovalWhoseAnswerWasLostEndsOnBothDevices)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    const auto unsyncedCredentialCount = [&]()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK);
        return credentials.size();
    };

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findCredential(), Database::FindResult::FOUND); });

    laptop_->device.run([&]() { ASSERT_TRUE(CredentialStore().remove(credential_id)); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);
    settle();

    phone_->device.run([&]() { ASSERT_EQ(findCredential(), Database::FindResult::NOT_FOUND); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "laptop";
        EXPECT_EQ(unsyncedCredentialCount(), 0) << "laptop";
    });

    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "phone"; });

    for (SyncWorker::Event event : laptop_->device.events())
        EXPECT_EQ(event, SyncWorker::Event::STORAGE_ATTACHED);
}

//--------------------------------------------------------------------------------------------------
// A change of a credential reaches the router, and the session breaks before the laptop gets the
// answer. The phone removes the credential after that, and the laptop changes it once more before
// its next session. The removal is what the router holds, and both devices end without the
// credential.
TEST_F(SyncExchangeTest, ChangeMadeAfterALostAnswerGivesWayToARemovalOnAnotherDevice)
{
    laptop_->device.startSession();
    settle();

    qint64 credential_id = 0;
    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_id = credential.id();
        credential_guid = credential.guid();
    });

    const auto findCredential = [&]()
    {
        CredentialConfig credential;
        return CredentialStore().findByGuid(credential_guid, &credential);
    };

    const auto unsyncedCredentialCount = [&]()
    {
        QList<CredentialStore::Unsynced> credentials;
        EXPECT_EQ(CredentialStore().unsynced(&credentials), Database::ReadResult::OK);
        return credentials.size();
    };

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    laptop_->device.run([&]() { setUsername("first"); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);
    settle();

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        ASSERT_EQ(credential.username().toStdString(), "first");
        ASSERT_TRUE(CredentialStore().remove(credential.id()));
    });

    phone_->device.startExchange();
    settle();

    laptop_->device.run([&]() { setUsername("second"); });

    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "laptop";
        EXPECT_EQ(unsyncedCredentialCount(), 0) << "laptop";
    });

    phone_->device.run([&]() { EXPECT_EQ(findCredential(), Database::FindResult::NOT_FOUND) << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The laptop is sending its hosts, when a new credential is made on it and a host is linked to the
// credential. The host goes to the router at once, and the credential only with the next exchange.
// The phone reads the host in between. Once the credential comes, the host has to link it on the
// phone as it does on the laptop.
TEST_F(SyncExchangeTest, HostSentBeforeTheNewCredentialItLinksEndsLinkedOnBothDevices)
{
    laptop_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        ASSERT_TRUE(RouterHostStore().add(host));
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    laptop_->device.run([&]()
    {
        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setUsername("another");
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    laptop_->device.startExchange();
    carryTillRecordsAreSent(*laptop_, proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS);

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();

        RouterHostConfig host;
        ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);

        host.setCredentialId(credential.id());
        ASSERT_TRUE(RouterHostStore().modify(host));
    });

    laptop_->device.startExchange();

    // The host as it was read to be sent, and then the host with the link.
    ASSERT_TRUE(carryRequest(*laptop_));
    ASSERT_TRUE(carryRequest(*laptop_));

    // The phone exchanges, while the laptop has not sent the credential yet.
    for (;;)
    {
        if (carryRequest(*phone_))
            continue;

        if (phone_->notifications.isEmpty())
            break;

        phone_->device.receive(phone_->notifications.takeFirst());
    }

    phone_->device.run([&]()
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::NOT_FOUND);
    });

    settle();

    for (Session* session : { laptop_.get(), phone_.get() })
    {
        const char* device_name = session == laptop_.get() ? "laptop" : "phone";

        session->device.run([&]()
        {
            CredentialConfig credential;
            ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND)
                << device_name;

            RouterHostConfig host;
            ASSERT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND) << device_name;
            EXPECT_EQ(host.username().toStdString(), "another") << device_name;
            EXPECT_EQ(host.credentialId(), credential.id()) << device_name;
        });
    }
}

//--------------------------------------------------------------------------------------------------
// A change of a credential reaches the router, and the session breaks before the laptop gets the
// answer. Nothing is changed on the laptop after that. The phone gets the change, goes offline, and
// the credential is changed there. The laptop comes back first and the phone after it. The change of
// the phone is the only one made since the router got the credential, so both devices have to end
// with it.
TEST_F(SyncExchangeTest, LostAnswerLeavesALaterChangeOfAnotherDevice)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    const auto storedUsername = [&]()
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        return credential.username().toStdString();
    };

    laptop_->device.run([&]() { setUsername("first"); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));
    breakSessionAtRequest(*laptop_);
    settle();

    phone_->device.run([&]() { ASSERT_EQ(storedUsername(), "first"); });

    endSession(*phone_);
    phone_->device.run([&]() { setUsername("phone"); });

    laptop_->device.startSession();
    settle();

    phone_->device.startSession();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(storedUsername(), "phone") << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The laptop sends a change of a credential, and the session breaks while the request is on its
// way. The credential is changed on the laptop once more. In the next session the laptop has read
// the credentials and is about to send the later change, when the request of the broken session
// reaches the router. The later change is refused then, and it is still the one both devices have to
// end with.
TEST_F(SyncExchangeTest, ChangeMadeAfterARequestThatComesLateIsKept)
{
    laptop_->device.startSession();
    settle();

    QString credential_guid;

    laptop_->device.run([&]()
    {
        CredentialConfig credential;
        credential.setDisplayName("office");
        credential.setUsername("admin");
        credential.setPassword(SecureString(QString("password")));
        ASSERT_TRUE(CredentialStore().add(credential));
        ASSERT_EQ(CredentialStore().find(credential.id(), &credential), Database::FindResult::FOUND);
        credential_guid = credential.guid();
    });

    laptop_->device.startExchange();
    settle();

    phone_->device.startSession();
    settle();

    const auto setUsername = [&](const QString& username)
    {
        CredentialConfig credential;
        ASSERT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);

        credential.setUsername(username);
        ASSERT_TRUE(CredentialStore().modify(credential));
    };

    const auto storedUsername = [&]()
    {
        CredentialConfig credential;
        EXPECT_EQ(CredentialStore().findByGuid(credential_guid, &credential), Database::FindResult::FOUND);
        return credential.username().toStdString();
    };

    laptop_->device.run([&]() { setUsername("first"); });

    laptop_->device.startExchange();
    ASSERT_TRUE(carryRequest(*laptop_));

    QByteArray late_request;
    ASSERT_TRUE(laptop_->device.takeSentMessage(&late_request));

    proto::router::SyncToRouter late_message;
    ASSERT_TRUE(parse(late_request, &late_message));
    ASSERT_TRUE(late_message.has_record_request());

    endSession(*laptop_);

    laptop_->device.run([&]() { setUsername("second"); });

    laptop_->device.startSession();
    carryTillRecordsAreSent(*laptop_, proto::router::SYNC_DATA_TYPE_CREDENTIALS);

    // The request of the broken session reaches the router, and nobody waits for the answer.
    QByteArray late_answer;
    ASSERT_NO_FATAL_FAILURE(answerSyncMessage(*laptop_, late_request, &late_answer));

    settle();

    // The laptop exchanges once more, the way it does when the router has refused a record.
    laptop_->device.startExchange();
    settle();

    laptop_->device.run([&]() { EXPECT_EQ(storedUsername(), "second") << "laptop"; });
    phone_->device.run([&]() { EXPECT_EQ(storedUsername(), "second") << "phone"; });
}

//--------------------------------------------------------------------------------------------------
// The same credential is changed on both devices.
TEST_F(SyncExchangeScheduleTest, CredentialChangedOnBothDevices)
{
    runEverySchedule({ { [this]() { editCredential("laptop"); } }, { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is removed on one device, and the host linking it is changed on the other.
TEST_F(SyncExchangeScheduleTest, CredentialRemovedWhileItsHostIsChanged)
{
    runEverySchedule({ { [this]() { removeCredential(); } }, { [this]() { editHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is removed on one device and changed on the other.
TEST_F(SyncExchangeScheduleTest, CredentialRemovedWhileItIsChanged)
{
    runEverySchedule({ { [this]() { removeCredential(); } }, { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A host is given a new credential on one device, and its pair is changed on the other.
TEST_F(SyncExchangeScheduleTest, HostLinkedToANewCredentialWhileItIsChanged)
{
    runEverySchedule({ { [this]() { addCredentialAndLinkHost("home"); } }, { [this]() { editHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A host is removed on one device and changed on the other.
TEST_F(SyncExchangeScheduleTest, HostRemovedWhileItIsChanged)
{
    runEverySchedule({ { [this]() { removeHost(); } }, { [this]() { editHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A host is removed and entered anew on one device, and its credential is changed on the other.
TEST_F(SyncExchangeScheduleTest, HostEnteredAnewWhileItsCredentialIsChanged)
{
    runEverySchedule({ { [this]() { removeHost(); }, [this]() { addHost("laptop"); } },
                       { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is changed and then removed on one device, and changed on the other.
TEST_F(SyncExchangeScheduleTest, CredentialChangedAndRemovedWhileItIsChanged)
{
    runEverySchedule({ { [this]() { editCredential("laptop"); }, [this]() { removeCredential(); } },
                       { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is removed on both devices.
TEST_F(SyncExchangeScheduleTest, CredentialRemovedOnBothDevices)
{
    runEverySchedule({ { [this]() { removeCredential(); } }, { [this]() { removeCredential(); } } });
}

//--------------------------------------------------------------------------------------------------
// A host is changed twice on one device and once on the other.
TEST_F(SyncExchangeScheduleTest, HostChangedOnBothDevices)
{
    runEverySchedule({ { [this]() { editHost("laptop"); }, [this]() { editHost("laptop again"); } },
                       { [this]() { editHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A host is given a new credential on one device and loses its link on the other.
TEST_F(SyncExchangeScheduleTest, HostLinkedToANewCredentialWhileItIsUnlinked)
{
    runEverySchedule({ { [this]() { addCredentialAndLinkHost("home"); } }, { [this]() { unlinkHost(); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is added and then removed on one device, and another one is changed on the other.
TEST_F(SyncExchangeScheduleTest, CredentialAddedAndRemovedWhileAnotherIsChanged)
{
    runEverySchedule({ { [this]() { addCredential("home"); }, [this]() { removeAddedCredential(); } },
                       { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A credential is removed and the host linking it is changed on one device, and the credential is
// changed on the other.
TEST_F(SyncExchangeScheduleTest, CredentialRemovedAndItsHostChangedWhileTheCredentialIsChanged)
{
    runEverySchedule({ { [this]() { removeCredential(); }, [this]() { editHost("laptop"); } },
                       { [this]() { editCredential("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// The same group is renamed on both devices.
TEST_F(SyncExchangeScheduleTest, GroupChangedOnBothDevices)
{
    runEverySchedule({ { [this]() { renameGroup(office_guid_, "laptop"); } },
                       { [this]() { renameGroup(office_guid_, "phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device and renamed on the other.
TEST_F(SyncExchangeScheduleTest, GroupRemovedWhileItIsChanged)
{
    runEverySchedule({ { [this]() { removeGroup(office_guid_); } },
                       { [this]() { renameGroup(office_guid_, "phone"); } }, Removal::ON_LAPTOP });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device, and a group is added to it on the other. With two deviations
// the removal is confirmed before the added group comes, and that group waits for a parent that
// is gone.
TEST_F(SyncExchangeScheduleTest, GroupRemovedWhileAGroupIsAddedToIt)
{
    runEverySchedule({ { [this]() { removeGroup(office_guid_); } },
                       { [this]() { addGroup("lab", office_guid_); } }, Removal::ON_LAPTOP, 2 });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device, and a host is added to a group below it on the other.
TEST_F(SyncExchangeScheduleTest, GroupRemovedWhileAHostIsAddedBelowIt)
{
    runEverySchedule({ { [this]() { removeGroup(office_guid_); } },
                       { [this]() { addLocalHost("bench", branch_guid_); } }, Removal::ON_LAPTOP });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device, and a host of a group below it is renamed on the other.
TEST_F(SyncExchangeScheduleTest, GroupRemovedWhileAHostBelowItIsChanged)
{
    runEverySchedule({ { [this]() { removeGroup(office_guid_); } },
                       { [this]() { renameLocalHost("phone"); } }, Removal::ON_LAPTOP });
}

//--------------------------------------------------------------------------------------------------
// Each of two groups is moved into the other one, every move on its own device.
TEST_F(SyncExchangeScheduleTest, GroupsMovedIntoEachOther)
{
    runEverySchedule({ { [this]() { moveGroup(office_guid_, home_guid_); } },
                       { [this]() { moveGroup(home_guid_, office_guid_); } } });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device, and another group is moved into it on the other.
TEST_F(SyncExchangeScheduleTest, GroupMovedIntoAGroupThatIsRemoved)
{
    runEverySchedule({ { [this]() { removeGroup(home_guid_); } },
                       { [this]() { moveGroup(office_guid_, home_guid_); } } });
}

//--------------------------------------------------------------------------------------------------
// The same local host is renamed on both devices.
TEST_F(SyncExchangeScheduleTest, LocalHostChangedOnBothDevices)
{
    runEverySchedule({ { [this]() { renameLocalHost("laptop"); } }, { [this]() { renameLocalHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A local host is removed on one device and renamed on the other.
TEST_F(SyncExchangeScheduleTest, LocalHostRemovedWhileItIsChanged)
{
    runEverySchedule({ { [this]() { removeLocalHost(); } }, { [this]() { renameLocalHost("phone"); } } });
}

//--------------------------------------------------------------------------------------------------
// A group is removed on one device, and a local host is moved into it on the other.
TEST_F(SyncExchangeScheduleTest, LocalHostMovedIntoAGroupThatIsRemoved)
{
    runEverySchedule({ { [this]() { removeGroup(home_guid_); } }, { [this]() { moveLocalHost(home_guid_); } } });
}

//--------------------------------------------------------------------------------------------------
// The scenarios below swap the devices of the ones above. The unhurried schedule carries the
// requests of the laptop first, so here the device that removes reaches the router second.
TEST_F(SyncExchangeScheduleTest, GroupChangedWhileItIsRemoved)
{
    runEverySchedule({ { [this]() { renameGroup(office_guid_, "laptop"); } },
                       { [this]() { removeGroup(office_guid_); } }, Removal::ON_PHONE });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupAddedToAGroupWhileTheGroupIsRemoved)
{
    runEverySchedule({ { [this]() { addGroup("lab", office_guid_); } },
                       { [this]() { removeGroup(office_guid_); } }, Removal::ON_PHONE });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostAddedBelowAGroupWhileTheGroupIsRemoved)
{
    runEverySchedule({ { [this]() { addLocalHost("bench", branch_guid_); } },
                       { [this]() { removeGroup(office_guid_); } }, Removal::ON_PHONE });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostBelowAGroupChangedWhileTheGroupIsRemoved)
{
    runEverySchedule({ { [this]() { renameLocalHost("laptop"); } },
                       { [this]() { removeGroup(office_guid_); } }, Removal::ON_PHONE });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupMovedIntoAGroupWhileTheGroupIsRemoved)
{
    runEverySchedule({ { [this]() { moveGroup(office_guid_, home_guid_); } },
                       { [this]() { removeGroup(home_guid_); } } });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostChangedWhileItIsRemoved)
{
    runEverySchedule({ { [this]() { renameLocalHost("laptop"); } },
                       { [this]() { removeLocalHost(); } } });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostMovedIntoAGroupWhileTheGroupIsRemoved)
{
    runEverySchedule({ { [this]() { moveLocalHost(home_guid_); } },
                       { [this]() { removeGroup(home_guid_); } } });
}

//--------------------------------------------------------------------------------------------------
// The scenarios below are the ones of one device: what the user leaves on the laptop is what both
// devices have to end with, whatever happens to the session.
TEST_F(SyncExchangeScheduleTest, CredentialChangedTwice)
{
    runEverySchedule({ { [this]() { editCredential("first"); }, [this]() { editCredential("second"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, CredentialChangedAndRemoved)
{
    runEverySchedule({ { [this]() { editCredential("first"); }, [this]() { removeCredential(); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, CredentialAddedAndRemoved)
{
    runEverySchedule({ { [this]() { addCredential("home"); }, [this]() { removeAddedCredential(); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, HostRemovedAndEnteredAnew)
{
    runEverySchedule({ { [this]() { removeHost(); }, [this]() { addHost("another"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, HostChangedTwice)
{
    runEverySchedule({ { [this]() { editHost("first"); }, [this]() { editHost("second"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, HostLinkedToANewCredentialAndChanged)
{
    runEverySchedule({ { [this]() { addCredentialAndLinkHost("home"); }, [this]() { editHost("another"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, CredentialRemovedAndItsHostChanged)
{
    runEverySchedule({ { [this]() { removeCredential(); }, [this]() { editHost("another"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, HostUnlinkedAndLinkedToANewCredential)
{
    runEverySchedule({ { [this]() { unlinkHost(); }, [this]() { addCredentialAndLinkHost("home"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupAddedAndGivenAHost)
{
    runEverySchedule({ { [this]() { addGroup("lab", QString()); },
                         [this]() { addLocalHost("bench", added_group_guid_); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupAddedAndRemoved)
{
    runEverySchedule({ { [this]() { addGroup("lab", QString()); },
                         [this]() { removeGroup(added_group_guid_); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupChangedAndRemoved)
{
    runEverySchedule({ { [this]() { renameGroup(office_guid_, "main"); },
                         [this]() { removeGroup(office_guid_); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, GroupMovedAndChanged)
{
    runEverySchedule({ { [this]() { moveGroup(office_guid_, home_guid_); },
                         [this]() { renameGroup(office_guid_, "main"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
// The moved group is sent ahead of the new one, so the other device gets it before its parent.
TEST_F(SyncExchangeScheduleTest, GroupMovedIntoANewGroup)
{
    runEverySchedule({ { [this]() { moveGroupIntoNewGroup(branch_guid_, "lab"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostMovedAndChanged)
{
    runEverySchedule({ { [this]() { moveLocalHost(home_guid_); }, [this]() { renameLocalHost("another"); } }, {} });
}

//--------------------------------------------------------------------------------------------------
TEST_F(SyncExchangeScheduleTest, LocalHostChangedAndRemoved)
{
    runEverySchedule({ { [this]() { renameLocalHost("another"); }, [this]() { removeLocalHost(); } }, {} });
}
