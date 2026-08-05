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

} // namespace

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
// A host whose removal was scheduled while it was offline comes back and is told to remove itself.
// It must NOT be announced: the record is gone from every list, so nothing else would stop a client
// that still knows the id from asking for a connection offer to a host the administrator removed.
TEST_F(HostNGTest, HostWithPendingRemovalIsNotAnnounced)
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

        EXPECT_TRUE(announced_.isEmpty());
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
