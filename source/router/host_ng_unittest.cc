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

#include "router/host_ng.h"

#include <optional>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/generic_hash.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"
#include "router/fake_tcp_channel.h"
#include "router/router_test_base.h"
#include "router/router_test_worker.h"

namespace {

const char kHostKey[] = "the-key-the-host-presents";
const char kHardwareId[] = "hw-id-of-the-host";

const Seconds kTelemetryInterval{ 5 };

const char kFirstTelemetry[] = "{\"version\":1,\"update\":{\"channel\":\"stable\"}}";
const char kSecondTelemetry[] = "{\"version\":1,\"update\":{\"channel\":\"beta\"}}";

} // namespace

// Hands the host a telemetry report with a synthetic clock. In production the report is read with
// the real time, and the tests cannot wait the real interval out.
class HostNGTestPeer
{
public:
    static void readTelemetry(HostNG& host, const std::string& json, TimePoint now)
    {
        proto::router::HostTelemetry telemetry;
        telemetry.set_json(json);
        host.readHostTelemetry(telemetry, now);
    }
};

// A host connection end to end: what it answers to an id request, which hosts it announces to the
// rest of the router and what it leaves behind when it goes away. The host is the real HostNG, in a
// real worker thread, against a real database - only the socket is a stand-in.
class HostNGTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        std::unique_ptr<RouterTestWorker> worker = std::make_unique<RouterTestWorker>(file_path_);
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    // What the router stores for a host is the hash of the key the host presents.
    static QByteArray keyHash(std::string_view key)
    {
        return GenericHash::hash(GenericHash::Type::BLAKE2b512, key);
    }

    // Creates the host in the worker thread, runs |body| there and destroys it there. The channel
    // belongs to the host, so it goes away with it.
    void withHost(const std::function<void(HostNG&, FakeTcpChannel*)>& body)
    {
        worker_->invoke([&]()
        {
            FakeTcpChannel* channel = new FakeTcpChannel();

            // A host authenticates anonymously: it has no user, only the peer facts the
            // authenticator collected.
            channel->setPeer(0, std::string(), proto::router::SESSION_TYPE_HOST, kVersion_3_0_0);

            HostNG host(worker_->database(), channel, nullptr);

            announced_.clear();
            QObject::connect(&host, &HostNG::sig_hostIdAssigned, &host,
                             [this](HostId host_id) { announced_.append(host_id); });

            body(host, channel);
        });
    }

    static QByteArray idRequest(proto::router::HostIdRequest::Type type, const char* key,
                                const char* hw_id = kHardwareId)
    {
        proto::router::HostToRouter message;
        proto::router::HostIdRequest* request = message.mutable_host_id_request();
        request->set_type(type);
        request->set_hw_id(hw_id);
        if (key)
            request->set_key(key);
        return serialize(message);
    }

    static QByteArray telemetry(const std::string& json)
    {
        proto::router::HostToRouter message;
        message.mutable_host_telemetry()->set_json(json);
        return serialize(message);
    }

    bool hasTelemetry(HostId host_id, const QString& telemetry)
    {
        return countRaw(QString("SELECT COUNT(*) FROM hosts WHERE id=%1 AND telemetry='%2'")
                            .arg(host_id).arg(telemetry)) == 1;
    }

    // The reply the host was sent last, parsed as RouterToHost.
    static std::optional<proto::router::RouterToHost> lastMessage(const FakeTcpChannel* channel)
    {
        if (channel->sent().isEmpty())
            return std::nullopt;

        proto::router::RouterToHost message;
        if (!parse(channel->sent().back().buffer, &message))
            return std::nullopt;
        return message;
    }

    bool hasPendingRemoval(HostId host_id)
    {
        return countRaw(QString("SELECT COUNT(*) FROM hosts_remove WHERE host_id=%1")
                            .arg(host_id)) > 0;
    }

    // Host ids announced to the rest of the router. Announcing is what puts a host
    // among the ones a client may be offered a connection to.
    QList<HostId> announced_;

    WorkerManager workers_;
    RouterTestWorker* worker_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
// A host nobody approved yet gets a temporary id and a key of its own, and nothing is written to
// the database: the record appears only when an administrator approves it.
TEST_F(HostNGTest, NewHostGetsATemporaryIdAndIsAnnounced)
{
    withHost([this](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::NEW_ID, nullptr));

        const std::optional<proto::router::RouterToHost> message = lastMessage(channel);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_host_id_response());
        EXPECT_EQ(message->host_id_response().error_code(), proto::router::kErrorOk);
        EXPECT_TRUE(isTempHostId(message->host_id_response().host_id()));
        EXPECT_FALSE(message->host_id_response().key().empty());

        ASSERT_EQ(announced_.size(), 1);
        EXPECT_EQ(announced_.at(0), host.hostId());
    });

    EXPECT_EQ(countRaw("SELECT COUNT(*) FROM hosts"), 0);
}

//--------------------------------------------------------------------------------------------------
// An approved host presents its key and gets the id that was issued to it.
TEST_F(HostNGTest, ApprovedHostGetsItsPermanentIdAndIsAnnounced)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([this, host_id](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        const std::optional<proto::router::RouterToHost> message = lastMessage(channel);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_host_id_response());
        EXPECT_EQ(message->host_id_response().error_code(), proto::router::kErrorOk);
        EXPECT_EQ(message->host_id_response().host_id(), host_id);

        ASSERT_EQ(announced_.size(), 1);
        EXPECT_EQ(announced_.at(0), host_id);
    });
}

//--------------------------------------------------------------------------------------------------
// An approved host has the telemetry it reports stored in its record, each report replacing the one
// before. A report over the size bound is dropped.
TEST_F(HostNGTest, TelemetryOfAnApprovedHostIsStored)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([&](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();

        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));
        channel->receive(0, telemetry(kFirstTelemetry));
        EXPECT_TRUE(hasTelemetry(host_id, kFirstTelemetry));

        const TimePoint later = Clock::now() + kTelemetryInterval;
        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, later);
        HostNGTestPeer::readTelemetry(host, std::string(8192, 'x'), later + kTelemetryInterval);
    });

    EXPECT_TRUE(hasTelemetry(host_id, kSecondTelemetry));
}

//--------------------------------------------------------------------------------------------------
// A report that repeats the one already stored over the same connection is not written again.
TEST_F(HostNGTest, RepeatedTelemetryIsNotWrittenAgain)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([&](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        const TimePoint now = Clock::now();
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, now);

        // A write would put the report back over the marker.
        ASSERT_TRUE(execRaw(QString("UPDATE hosts SET telemetry='marker' WHERE id=%1").arg(host_id)));
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, now + kTelemetryInterval);
        EXPECT_TRUE(hasTelemetry(host_id, "marker"));

        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, now + 2 * kTelemetryInterval);
    });

    EXPECT_TRUE(hasTelemetry(host_id, kSecondTelemetry));
}

//--------------------------------------------------------------------------------------------------
// A report that comes sooner than the interval after the one accepted before it is dropped, so a
// host cannot keep the router writing to the database.
TEST_F(HostNGTest, TooFrequentTelemetryIsDropped)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([&](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        const TimePoint now = Clock::now();
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, now);

        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, now + kTelemetryInterval - Seconds(1));
        EXPECT_TRUE(hasTelemetry(host_id, kFirstTelemetry));

        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, now + kTelemetryInterval);
        EXPECT_TRUE(hasTelemetry(host_id, kSecondTelemetry));
    });
}

//--------------------------------------------------------------------------------------------------
// A repeat of the stored report does not hold the next one back. Saving the host settings sends a
// report with the old values first and one with the new values right after it, and the second one
// must not be dropped as too frequent.
TEST_F(HostNGTest, RepeatedTelemetryDoesNotHoldTheNextOne)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([&](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        const TimePoint now = Clock::now();
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, now);

        const TimePoint later = now + kTelemetryInterval;
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, later);
        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, later + Seconds(1));
        EXPECT_TRUE(hasTelemetry(host_id, kSecondTelemetry));
    });
}

//--------------------------------------------------------------------------------------------------
// A reconnected host that reports what is already in the database causes no write. The report does
// not hold the interval either, so a changed report right after it is taken.
TEST_F(HostNGTest, StoredTelemetryIsNotWrittenAgainOnReconnect)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_TRUE(db_.updateHostTelemetry(host_id, kFirstTelemetry));

    withHost([&](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        const TimePoint now = Clock::now();
        HostNGTestPeer::readTelemetry(host, kFirstTelemetry, now);
        HostNGTestPeer::readTelemetry(host, kSecondTelemetry, now + Seconds(1));
        EXPECT_TRUE(hasTelemetry(host_id, kSecondTelemetry));
    });
}

//--------------------------------------------------------------------------------------------------
// A host whose removal was scheduled while it was offline comes back and is told to remove itself.
// The command goes out before the id is reported to the worker, which is what keeps the host out of
// the reachable ones - the record is gone from every list, so nothing else would stop a client that
// still knows the id from asking for a connection offer to a host the administrator removed.
TEST_F(HostNGTest, HostWithPendingRemovalIsToldBeforeItsIdIsReported)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    withHost([this, host_id](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));

        // The host still learns which id is being removed, and gets the command.
        ASSERT_EQ(channel->sent().size(), 2);

        proto::router::RouterToHost response;
        ASSERT_TRUE(parse(channel->sent().at(0).buffer, &response));
        ASSERT_TRUE(response.has_host_id_response());
        EXPECT_EQ(response.host_id_response().host_id(), host_id);

        proto::router::RouterToHost command;
        ASSERT_TRUE(parse(channel->sent().at(1).buffer, &command));
        ASSERT_TRUE(command.has_host_command());
        EXPECT_EQ(command.host_command().command_name(), proto::router::kCommandHostRemove);

        // The id is reported all the same, so a stale predecessor of the same host is dropped
        // in favour of this one, and the connection carries the mark that keeps it unannounced.
        EXPECT_EQ(announced_, QList<HostId>({ host_id }));
        EXPECT_TRUE(host.isRemoveCommandSent());
    });
}

//--------------------------------------------------------------------------------------------------
// The removal is finished when the connection goes away: the host was told, and whether it acted on
// the command or simply dropped, the id will not come back.
TEST_F(HostNGTest, RemovalIsFinalizedWhenTheHostGoesAway)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));
    ASSERT_TRUE(hasPendingRemoval(host_id));

    withHost([](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));
    });

    EXPECT_FALSE(hasPendingRemoval(host_id));
    EXPECT_EQ(countRaw("SELECT COUNT(*) FROM hosts"), 0);
}

//--------------------------------------------------------------------------------------------------
// A host asks for its id once per connection. A repeat is ignored: an untrusted host must not be
// able to swap the id it was already given.
TEST_F(HostNGTest, RepeatedIdRequestIsIgnored)
{
    const HostId host_id = addHost(toStdString(keyHash(kHostKey)));
    ASSERT_NE(host_id, kInvalidHostId);

    withHost([this, host_id](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, kHostKey));
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::NEW_ID, nullptr));

        EXPECT_TRUE(channel->nothingSent());
        EXPECT_EQ(host.hostId(), host_id);
        EXPECT_EQ(announced_.size(), 1);
    });
}

//--------------------------------------------------------------------------------------------------
// The hardware id is what the router stores to recognize the machine later. A host that reports
// none is not served at all.
TEST_F(HostNGTest, HostWithoutAHardwareIdIsDisconnected)
{
    withHost([](HostNG& host, FakeTcpChannel* channel)
    {
        int finished = 0;
        QObject::connect(&host, &HostNG::sig_finished, &host,
                         [&finished](qint64) { ++finished; });

        host.start();
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::NEW_ID, nullptr, ""));

        EXPECT_EQ(finished, 1);
        EXPECT_TRUE(channel->nothingSent());
    });
}

//--------------------------------------------------------------------------------------------------
// A key that belongs to no host is answered with "not found" instead of an id, and the host is not
// announced.
TEST_F(HostNGTest, UnknownKeyIsRefused)
{
    withHost([this](HostNG& host, FakeTcpChannel* channel)
    {
        host.start();
        channel->clearSent();

        channel->receive(0, idRequest(proto::router::HostIdRequest::EXISTING_ID, "no-such-key"));

        const std::optional<proto::router::RouterToHost> message = lastMessage(channel);
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_host_id_response());
        EXPECT_EQ(message->host_id_response().error_code(), proto::router::kErrorNotFound);
        EXPECT_EQ(message->host_id_response().host_id(), kInvalidHostId);
        EXPECT_TRUE(announced_.isEmpty());
    });
}
