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

#ifndef ROUTER_LOOPBACK_CHANNEL_H
#define ROUTER_LOOPBACK_CHANNEL_H

#include <QCoreApplication>
#include <QList>
#include <QPointer>

#include <asio/ip/tcp.hpp>
#include <gtest/gtest.h>

#include <functional>

#include "base/logging.h"
#include "base/shared_pointer.h"
#include "base/time_types.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel_native.h"
#include "base/net/tcp_server.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/router_user.h"
#include "base/peer/user_list.h"
#include "proto/key_exchange.h"

// Both ends of a real channel on the loopback interface, living in the thread of the caller. The
// session under test is given the router end and the test keeps the peer end. What the session
// sends is collected at the peer end, and what a peer would send goes out through it. The calls
// that look at the sent messages first wait until both ends have handled every byte in flight, so
// a test still reads as a sequence of steps.
class LoopbackChannel
{
public:
    struct Sent
    {
        quint8 channel_id = 0;
        QByteArray buffer;
    };

    // A peer that logs in as |user|, the way an operator, a manager or an administrator does.
    LoopbackChannel(const RouterUser& user, const QString& password, quint32 session_type)
    {
        User account = user;
        account.sessions = RouterUser::expandSessionTypes(account.sessions);

        ClientAuthenticator* authenticator = new ClientAuthenticator();
        authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
        authenticator->setUserName(user.name);
        authenticator->setPassword(SecureString(password));
        authenticator->setSessionType(session_type);

        TcpServer server;
        server.setUserList(SharedPointer<SingleUserList>(new SingleUserList(account)));

        connectTo(&server, authenticator);
    }

    // A peer that connects anonymously, the way hosts and relays do.
    explicit LoopbackChannel(quint32 session_type)
    {
        const KeyPair keys = KeyPair::create(KeyPair::Type::X25519);
        CHECK(keys.isValid());

        ClientAuthenticator* authenticator = new ClientAuthenticator();
        authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
        authenticator->setPeerPublicKey(keys.publicKey());
        authenticator->setSessionType(session_type);

        TcpServer server;
        server.setPrivateKey(SecureByteArray(keys.privateKey()));
        server.setAnonymousAccess(ServerAuthenticator::AnonymousAccess::ENABLE, session_type);

        connectTo(&server, authenticator);
    }

    ~LoopbackChannel()
    {
        delete peer_;
    }

    // The router end, or nullptr when the handshake failed. The session it is given to owns it.
    TcpChannel* routerEnd() const { return router_; }

    // Delivers a message to the session, as a peer would send it.
    void receive(quint8 channel_id, const QByteArray& buffer)
    {
        if (!peer_)
            return;

        peer_->send(channel_id, buffer);
        settle();
    }

    // The peer drops the connection. Returns once the router end has reported it.
    void close()
    {
        delete peer_;
        peer_ = nullptr;

        waitFor([this]() { return !router_ || router_failed_; });
    }

    // Waits until both ends have handled everything in flight. A paused router end does not read,
    // so what the peer sent to it keeps waiting in the socket.
    void settle()
    {
        waitFor([this]()
        {
            if (!peer_ || !router_ || peer_failed_ || router_failed_)
                return true;

            return peer_->pendingBytes() == 0 && router_->pendingBytes() == 0 &&
                   peer_->totalRx() == router_->totalTx() &&
                   (router_->isPaused() || router_->totalRx() == peer_->totalTx());
        });
    }

    const QList<Sent>& sent()
    {
        settle();
        return sent_;
    }

    bool nothingSent() { return sent().isEmpty(); }

    void clearSent()
    {
        settle();
        sent_.clear();
    }

    bool isPaused() const { return router_ && router_->isPaused(); }

private:
    // The router looks the user up in its database. Here the record comes from the test.
    class SingleUserList final : public UserList
    {
    public:
        explicit SingleUserList(const User& user)
            : user_(user)
        {
            // Nothing
        }

        // UserList implementation.
        User find(const QString& username) const final
        {
            if (username.compare(user_.name, Qt::CaseInsensitive) != 0)
                return User::kInvalidUser;
            return user_;
        }

        QByteArray seedKey() const final { return seed_key_; }
        void setSeedKey(const QByteArray& seed_key) final { seed_key_ = seed_key; }

    private:
        const User user_;
        QByteArray seed_key_ { "seed-key-for-tests" };
    };

    void connectTo(TcpServer* server, ClientAuthenticator* authenticator)
    {
        peer_ = new TcpChannelNative(authenticator);

        const quint16 port = freePort();
        if (!server->start(port, "127.0.0.1"))
        {
            ADD_FAILURE() << "Unable to listen on port " << port;
            return;
        }

        QObject::connect(peer_, &TcpChannel::sig_authenticated, &receiver_, [this]()
        {
            peer_->setPaused(false);
            peer_authenticated_ = true;
        });

        QObject::connect(peer_, &TcpChannel::sig_messageReceived, &receiver_,
                         [this](quint8 channel_id, const QByteArray& buffer)
        {
            sent_.append({ channel_id, buffer });
        });

        QObject::connect(peer_, &TcpChannel::sig_errorOccurred, &receiver_,
                         [this](TcpChannel::ErrorCode /* error_code */)
        {
            peer_failed_ = true;
        });

        peer_->connectTo("127.0.0.1", port);

        waitFor([&]() { return peer_failed_ || (peer_authenticated_ && server->hasReadyConnections()); });

        router_ = server->nextReadyConnection();
        if (!router_)
        {
            ADD_FAILURE() << "The handshake did not complete";
            return;
        }

        QObject::connect(router_, &TcpChannel::sig_errorOccurred, &receiver_,
                         [this](TcpChannel::ErrorCode /* error_code */)
        {
            router_failed_ = true;
        });
    }

    // Runs the event loop of the thread until |done| holds. The deadline exists to fail a broken
    // test instead of hanging forever.
    static void waitFor(const std::function<bool()>& done)
    {
        const TimePoint deadline = Clock::now() + Seconds(30);

        while (!done())
        {
            if (Clock::now() >= deadline)
            {
                ADD_FAILURE() << "Timed out waiting for the loopback channel";
                return;
            }

            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
    }

    // A free loopback port. It is released before the server binds it, and the window between the
    // two is harmless.
    static quint16 freePort()
    {
        asio::io_context io_context;
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);

        std::error_code error_code;
        asio::ip::tcp::acceptor acceptor(io_context);
        acceptor.open(endpoint.protocol(), error_code);
        CHECK(!error_code);
        acceptor.bind(endpoint, error_code);
        CHECK(!error_code);

        return acceptor.local_endpoint().port();
    }

    // The context of every connection to the ends. Destroying it disconnects them, so a router end
    // that outlives this object no longer calls back into it.
    QObject receiver_;

    TcpChannel* peer_ = nullptr;
    QPointer<TcpChannel> router_;
    QList<Sent> sent_;

    bool peer_authenticated_ = false;
    bool peer_failed_ = false;
    bool router_failed_ = false;

    Q_DISABLE_COPY_MOVE(LoopbackChannel)
};

#endif // ROUTER_LOOPBACK_CHANNEL_H
