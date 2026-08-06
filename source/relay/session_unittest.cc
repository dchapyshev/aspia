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

#include "relay/session.h"

#include <gtest/gtest.h>

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <memory>
#include <string>

#include "base/serialization.h"
#include "proto/relay_peer.h"
#include "relay/relay_test_worker.h"

namespace {

// The waits exist to fail a broken test instead of hanging forever, not to measure anything.
const Seconds kWaitTimeout{ 30 };

} // namespace

// An active session of the relay. Two paired peers stay on their sockets and the session pumps
// bytes between them, keeping the identity it was given for the statistics.
class SessionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::unique_ptr<RelayTestWorker> worker = std::make_unique<RelayTestWorker>();
        worker_ = worker.get();

        workers_.add(std::move(worker));
        workers_.start();
    }

    void TearDown() override
    {
        worker_->invoke([this]()
        {
            session_.reset();
            client_.reset();
            host_.reset();
        });
    }

    // Creates the session over two fresh connections and starts it. The far ends stay with the
    // test as |client_| and |host_|.
    void createSession(const QByteArray& secret)
    {
        worker_->invoke([&]()
        {
            auto [client_end, session_client_end] = RelayTestWorker::createSocketPair();
            auto [host_end, session_host_end] = RelayTestWorker::createSocketPair();

            client_ = std::make_unique<asio::ip::tcp::socket>(std::move(client_end));
            host_ = std::make_unique<asio::ip::tcp::socket>(std::move(host_end));

            session_ = std::make_unique<Session>(
                std::make_pair(std::move(session_client_end), std::move(session_host_end)), secret);

            QObject::connect(session_.get(), &Session::sig_finished, session_.get(),
                             [this]() { finished_.signal(); });

            session_->start();
        });
    }

    // The secret both peers presented, as the pairing leaves it.
    static QByteArray secret(const std::string& client_user_name = "operator")
    {
        proto::relay::PeerToRelay::Secret message;
        message.set_client_address("203.0.113.5");
        message.set_client_user_name(client_user_name);
        message.set_host_address("198.51.100.7");
        message.set_host_id(100);
        return serialize(message);
    }

    void send(asio::ip::tcp::socket& socket, const QByteArray& data)
    {
        worker_->invoke([&]()
        {
            std::error_code error_code;
            asio::write(socket, asio::const_buffer(data.constData(), data.size()), error_code);
            CHECK(!error_code);
        });
    }

    // Reads exactly |size| bytes from |socket|. The read is asynchronous in the worker, so the
    // session keeps pumping while the test waits.
    QByteArray receive(asio::ip::tcp::socket& socket, qsizetype size)
    {
        received_data_.resize(size);
        const int expected = ++receive_count_;

        worker_->invoke([&]()
        {
            asio::async_read(socket, asio::buffer(received_data_.data(), received_data_.size()),
                             [this](const std::error_code&, size_t) { received_.signal(); });
        });

        if (!received_.wait(expected, kWaitTimeout))
        {
            ADD_FAILURE() << "Receive timed out";
            return QByteArray();
        }

        return received_data_;
    }

    WorkerManager workers_;
    RelayTestWorker* worker_ = nullptr;

    std::unique_ptr<asio::ip::tcp::socket> client_;
    std::unique_ptr<asio::ip::tcp::socket> host_;
    std::unique_ptr<Session> session_;

    QByteArray received_data_;
    int receive_count_ = 0;
    TestLatch received_;
    TestLatch finished_;
};

//--------------------------------------------------------------------------------------------------
// The whole point of the relay. Bytes written by one peer come out at the other, in both
// directions, and the counter the statistics report grows with them.
TEST_F(SessionTest, BytesFlowBothWays)
{
    createSession(secret());

    const QByteArray from_client = QByteArrayLiteral("client-to-host");
    send(*client_, from_client);
    EXPECT_EQ(receive(*host_, from_client.size()), from_client);

    const QByteArray from_host = QByteArrayLiteral("host-to-client!!");
    send(*host_, from_host);
    EXPECT_EQ(receive(*client_, from_host.size()), from_host);

    worker_->invoke([&]()
    {
        EXPECT_EQ(session_->bytesTransferred(), from_client.size() + from_host.size());
    });
}

//--------------------------------------------------------------------------------------------------
// What the administrator sees about a session comes from the secret the peers presented.
TEST_F(SessionTest, IdentityComesFromTheSecret)
{
    createSession(secret());

    worker_->invoke([this]()
    {
        EXPECT_EQ(session_->clientAddress(), "203.0.113.5");
        EXPECT_EQ(session_->clientUserName(), "operator");
        EXPECT_EQ(session_->hostAddress(), "198.51.100.7");
        EXPECT_EQ(session_->hostId(), HostId(100));
    });
}

//--------------------------------------------------------------------------------------------------
// The identity travels to the administrator inside the statistics report, so a field of an
// unbounded length is cut instead of being carried around.
TEST_F(SessionTest, OversizedIdentityFieldIsTruncated)
{
    createSession(secret(std::string(300, 'u')));

    worker_->invoke([this]()
    {
        EXPECT_EQ(session_->clientUserName().size(), 255);
    });
}

//--------------------------------------------------------------------------------------------------
// A secret that does not parse leaves the identity empty, and the session still relays.
TEST_F(SessionTest, BrokenSecretLeavesIdentityEmpty)
{
    createSession(QByteArray::fromHex("ffffffffffffffff"));

    worker_->invoke([this]()
    {
        EXPECT_TRUE(session_->clientAddress().isEmpty());
        EXPECT_TRUE(session_->clientUserName().isEmpty());
        EXPECT_EQ(session_->hostId(), kInvalidHostId);
    });

    const QByteArray data = QByteArrayLiteral("still-relayed");
    send(*client_, data);
    EXPECT_EQ(receive(*host_, data.size()), data);
}

//--------------------------------------------------------------------------------------------------
// Either peer going away finishes the session, and the owner removes it.
TEST_F(SessionTest, PeerDisconnectFinishesTheSession)
{
    createSession(secret());

    worker_->invoke([this]()
    {
        std::error_code ignored_code;
        client_->close(ignored_code);
    });

    ASSERT_TRUE(finished_.wait(1, kWaitTimeout));
}
