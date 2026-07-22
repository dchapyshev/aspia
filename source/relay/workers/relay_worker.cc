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

#include "relay/workers/relay_worker.h"

#include <QTimer>

#include "base/logging.h"
#include "base/crypto/stream_decryptor.h"
#include "base/net/flood_guard.h"
#include "base/threading/asio_event_dispatcher.h"
#include "proto/relay_peer.h"
#include "relay/pending_session.h"
#include "relay/session.h"
#include "relay/settings.h"
#include "relay/shared_key_pool.h"

namespace {

const Seconds kTimerInterval{ 1 };
const Minutes kIdleCheckInterval{ 1 };
constexpr Hours kMaxSessionDuration{ 24 };
constexpr Seconds kAcceptRetryDelay{ 1 };

// Caps for the relay role. The relay routes already-paired peers, so concurrent unfinished
// handshakes are usually few; a tight pending cap protects relay resources without rejecting
// normal traffic. The per-address rate matches the FloodGuard default but is set explicitly so
// the policy is visible at the use site.
constexpr Seconds kPerAddressWindow{ 60 };
constexpr int kPerAddressMax = 60;
constexpr int kMaxPendingSessions = 60;

// AEAD tag size (bytes) appended to every ciphertext. A valid encrypted secret is at least this
// long (empty plaintext plus tag); anything shorter would make the decrypted size negative.
constexpr qsizetype kAeadTagSize = 16;

//--------------------------------------------------------------------------------------------------
// Decrypts an encrypted pair of peer identifiers using key |key|.
QByteArray decryptSecret(const proto::relay::PeerToRelay& message, const SharedKeyPool::Key& key)
{
    if (key.first.isEmpty() || key.second.isEmpty())
    {
        LOG(ERROR) << "Invalid session key";
        return QByteArray();
    }

    // Peers that predate AES support leave the field unset (UNKNOWN), which means ChaCha20-Poly1305.
    std::unique_ptr<StreamDecryptor> decryptor;
    if (message.encryption() == proto::relay::PeerToRelay::ENCRYPTION_AES256_GCM)
        decryptor = StreamDecryptor::createForAes256Gcm(key.first, key.second);
    else
        decryptor = StreamDecryptor::createForChaCha20Poly1305(key.first, key.second);

    if (!decryptor)
    {
        LOG(ERROR) << "Decryptor not created";
        return QByteArray();
    }

    const std::string& source = message.data();
    if (static_cast<qsizetype>(source.size()) <= kAeadTagSize)
    {
        LOG(ERROR) << "Too short 'data' field:" << source.size();
        return QByteArray();
    }

    QByteArray target;
    target.resize(static_cast<qsizetype>(decryptor->decryptedDataSize(source.size())));

    if (!decryptor->decrypt(source.data(), source.size(), target.data()))
    {
        LOG(ERROR) << "Failed to decrypt shared secret";
        return QByteArray();
    }

    return target;
}

//--------------------------------------------------------------------------------------------------
QString peerAddress(const asio::ip::tcp::socket& socket)
{
    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(error_code);
        if (error_code)
        {
            LOG(ERROR) << "Unable to get endpoint for accepted connection:" << error_code;
        }
        else
        {
            std::string address = endpoint.address().to_string();
            return QString::fromLocal8Bit(address.c_str(), static_cast<int>(address.size()));
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Unable to get address for pending session:" << e.what();
    }

    return QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
RelayWorker::RelayWorker()
    : Worker(Thread::AsioDispatcher, kTimerInterval)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayWorker::~RelayWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onDisconnectSession(qint64 session_id)
{
    LOG(INFO) << "Disconnect session by session id:" << session_id;

    for (auto* session : std::as_const(active_sessions_))
    {
        if (session->sessionId() == session_id)
        {
            removeSession(session);
            return;
        }
    }

    LOG(ERROR) << "Session with id" << session_id << "is not found";
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onStart()
{
    Settings settings;

    quint16 peer_port = settings.peerPort();
    Minutes idle_timeout = settings.peerIdleTimeout();

    LOG(INFO) << "Peer port:" << peer_port;
    LOG(INFO) << "Peer idle timeout:" << idle_timeout.count();
    LOG(INFO) << "Statistics enabled:" << settings.isStatisticsEnabled();
    LOG(INFO) << "Statistics interval:" << settings.statisticsInterval().count();

    if (peer_port == 0)
    {
        LOG(ERROR) << "Invalid peer port";
        return;
    }

    if (idle_timeout < Minutes(1) || idle_timeout > Minutes(60))
    {
        LOG(ERROR) << "Invalid peer idle specified";
        return;
    }

    if (settings.isStatisticsEnabled())
    {
        Seconds interval = settings.statisticsInterval();
        if (interval < Seconds(1) || interval > Minutes(60))
        {
            LOG(ERROR) << "Invalid statistics interval";
            return;
        }
    }

    QString iface = settings.listenInterface();
    asio::ip::address address;
    if (!iface.isEmpty())
    {
        std::error_code error_code;
        address = asio::ip::make_address(iface.toLocal8Bit().toStdString(), error_code);
        if (error_code)
        {
            LOG(ERROR) << "Unable to get listen address:" << error_code;
            return;
        }
    }
    else
    {
        address = asio::ip::address_v6::any();
    }

    LOG(INFO) << "Listen interface:" << (iface.isEmpty() ? "ANY" : iface) << ":" << peer_port;

    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(AsioEventDispatcher::ioContext());

    asio::ip::tcp::endpoint endpoint(address, peer_port);

    std::error_code error_code;
    acceptor_->open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "open failed:" << error_code;
        acceptor_.reset();
        return;
    }

    acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(ERROR) << "set_option failed:" << error_code;
        acceptor_.reset();
        return;
    }

    acceptor_->bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "bind failed:" << error_code;
        acceptor_.reset();
        return;
    }

    acceptor_->listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(ERROR) << "listen failed:" << error_code;
        acceptor_.reset();
        return;
    }

    idle_timeout_ = idle_timeout;

    flood_guard_ = std::make_unique<FloodGuard>();
    flood_guard_->setRateLimit(kPerAddressWindow, kPerAddressMax);
    flood_guard_->setMaxPending(kMaxPendingSessions);

    start_time_ = Clock::now();
    next_idle_check_ = start_time_ + kIdleCheckInterval;

    if (settings.isStatisticsEnabled())
    {
        stat_interval_ = settings.statisticsInterval();
        next_stat_time_ = start_time_ + stat_interval_;
    }

    RelayWorker::doAccept(this);
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onStop()
{
    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;

    if (acceptor_)
    {
        std::error_code ignored_code;
        acceptor_->cancel(ignored_code);
        acceptor_->close(ignored_code);
        acceptor_.reset();
    }

    for (auto* session : std::as_const(pending_sessions_))
    {
        session->disconnect();
        delete session;
    }
    pending_sessions_.clear();

    for (auto* session : std::as_const(active_sessions_))
    {
        session->disconnect();
        delete session;
    }
    active_sessions_.clear();

    SharedKeyPool::instance().clear();
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onTimer(TimePoint now)
{
    // Pending sessions have a short handshake deadline (seconds), so sweep them on every tick rather
    // than on the coarse idle-check interval used for active sessions below.
    for (auto it = pending_sessions_.begin(); it != pending_sessions_.end();)
    {
        PendingSession* session = *it;
        if (session->isExpired(now))
        {
            LOG(INFO) << "Pending session timed out:" << session->address();
            session->disconnect();
            session->deleteLater();

            it = pending_sessions_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (now >= next_idle_check_)
    {
        next_idle_check_ = now + kIdleCheckInterval;

        int count = 0;
        for (auto it = active_sessions_.begin(); it != active_sessions_.end();)
        {
            Session* session = *it;

            const bool idle_expired = session->idleTime(now) >= idle_timeout_;
            const bool duration_expired = session->duration(now) >= kMaxSessionDuration;

            if (idle_expired || duration_expired)
            {
                session->disconnect();
                session->deleteLater();

                it = active_sessions_.erase(it);
                ++count;
                emit sig_sessionFinished();
            }
            else
            {
                ++it;
            }
        }

        if (count > 0)
            LOG(INFO) << "Sessions ended by timeout:" << count;
    }

    if (stat_interval_ > Seconds::zero() && now >= next_stat_time_)
    {
        next_stat_time_ = now + stat_interval_;

        proto::router::RelayStatistics statistics;
        statistics.set_uptime(DurationCast<Seconds>(now - start_time_).count());

        for (const auto& session : std::as_const(active_sessions_))
        {
            proto::router::Peer* peer = statistics.add_peer();
            peer->set_peer_id(session->sessionId());
            peer->set_status(proto::router::Peer::STATUS_ACTIVE);
            peer->set_client_address(session->clientAddress().toStdString());
            peer->set_client_user_name(session->clientUserName().toStdString());
            peer->set_host_address(session->hostAddress().toStdString());
            peer->set_host_id(session->hostId());
            peer->set_bytes_transferred(session->bytesTransferred());
            peer->set_idle_time(session->idleTime(now).count());
            peer->set_duration(session->duration(now).count());
        }

        emit sig_statistics(statistics);
    }
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onPendingSessionReady(const proto::relay::PeerToRelay& message)
{
    LOG(INFO) << "Pending session ready for key_id:" << message.key_id();

    PendingSession* pending_session = dynamic_cast<PendingSession*>(sender());
    CHECK(pending_session);

    // Looking for a key with the specified identifier.
    std::optional<SharedKeyPool::Key> key =
        SharedKeyPool::instance().find(message.key_id(), message.public_key());
    if (!key.has_value())
    {
        LOG(ERROR) << "Key with id" << message.key_id() << "is NOT found!";
        removePendingSession(pending_session);
        return;
    }

    // Decrypt the identifiers of peers.
    QByteArray secret = decryptSecret(message, *key);
    if (secret.isEmpty())
    {
        LOG(ERROR) << "Failed to decrypt shared secret. Connection will be terminated";
        removePendingSession(pending_session);
        return;
    }

    // Save the identifiers of peers and the identifier of their shared key.
    pending_session->setIdentify(message.key_id(), secret);

    // Trying to find a peer that wants to be connected.
    for (auto* other_pending_session : std::as_const(pending_sessions_))
    {
        if (!pending_session->isPeerFor(*other_pending_session))
            continue;

        LOG(INFO) << "Both peers are connected with key" << message.key_id();

        // Delete the key from the pool. It can no longer be used.
        SharedKeyPool::instance().remove(message.key_id());

        Session* session = new Session(
            std::make_pair(pending_session->takeSocket(), other_pending_session->takeSocket()),
            secret, this);
        active_sessions_.emplace_back(session);

        connect(session, &Session::sig_finished, this, &RelayWorker::onSessionFinished);

        // Now the opposite peer is found, start the data transfer between them.
        session->start();
        emit sig_sessionStarted();

        // Pending sessions are no longer needed, remove them.
        removePendingSession(other_pending_session);
        removePendingSession(pending_session);
        return;
    }

    LOG(INFO) << "Second peer has not connected yet";
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onPendingSessionFailed()
{
    PendingSession* session = dynamic_cast<PendingSession*>(sender());
    CHECK(session);
    removePendingSession(session);
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::onSessionFinished()
{
    Session* session = dynamic_cast<Session*>(sender());
    CHECK(session);
    removeSession(session);
}

//--------------------------------------------------------------------------------------------------
// static
void RelayWorker::doAccept(RelayWorker* self)
{
    auto guard = self->alive_guard_;
    self->acceptor_->async_accept([self, guard](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (!*guard)
            return;

        if (!error_code)
        {
            if (!self->flood_guard_->check(socket, self->pending_sessions_.size()))
            {
                LOG(TRACE) << "Connection rejected by flood guard:" << peerAddress(socket);
                // |socket| goes out of scope here - asio's destructor closes the descriptor.
            }
            else
            {
                LOG(INFO) << "New accepted connection:" << peerAddress(socket);

                PendingSession* session = new PendingSession(std::move(socket), self);
                self->pending_sessions_.emplace_back(session);

                connect(session, &PendingSession::sig_ready, self, &RelayWorker::onPendingSessionReady);
                connect(session, &PendingSession::sig_failed, self, &RelayWorker::onPendingSessionFailed);

                // A new peer is connected. Create and start the pending session.
                session->start();
            }
        }
        else
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(ERROR) << "Error while accepting connection:" << error_code;

            QTimer::singleShot(kAcceptRetryDelay, self, [self, guard]()
            {
                if (*guard)
                    RelayWorker::doAccept(self);
            });
            return;
        }

        // Waiting for the next connection.
        RelayWorker::doAccept(self);
    });
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::removePendingSession(PendingSession* session)
{
    session->disconnect();
    session->deleteLater();
    pending_sessions_.removeOne(session);
}

//--------------------------------------------------------------------------------------------------
void RelayWorker::removeSession(Session* session)
{
    session->disconnect();
    session->deleteLater();
    active_sessions_.removeOne(session);
    emit sig_sessionFinished();
}
