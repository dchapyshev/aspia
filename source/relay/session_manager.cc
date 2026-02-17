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

#include "relay/session_manager.h"

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/crypto/message_decryptor.h"

namespace relay {

namespace {

const std::chrono::minutes kIdleTimerInterval { 1 };

//--------------------------------------------------------------------------------------------------
// Decrypts an encrypted pair of peer identifiers using key |session_key|.
QByteArray decryptSecret(const proto::relay::PeerToRelay& message, const KeyPool::Key& key)
{
    if (key.first.isEmpty() || key.second.isEmpty())
    {
        LOG(ERROR) << "Invalid session key";
        return QByteArray();
    }

    std::unique_ptr<base::MessageDecryptor> decryptor =
        base::MessageDecryptor::createForChaCha20Poly1305(key.first, key.second);
    if (!decryptor)
    {
        LOG(ERROR) << "Decryptor not created";
        return QByteArray();
    }

    const std::string& source = message.data();
    if (source.empty())
    {
        LOG(ERROR) << "Empty 'data' field";
        return QByteArray();
    }

    QByteArray target;
    target.resize(static_cast<QByteArray::size_type>(decryptor->decryptedDataSize(source.size())));

    if (!decryptor->decrypt(source.data(), source.size(), target.data()))
    {
        LOG(ERROR) << "Failed to decrypt shared secret";
        return QByteArray();
    }

    return target;
}

//--------------------------------------------------------------------------------------------------
// Removes a session from the list.
template<class T>
void removeSessionT(QList<T*>* sessions, T* session_to_remove)
{
    session_to_remove->stop();

    auto it = sessions->begin();
    auto it_end = sessions->end();

    while (it != it_end)
    {
        T* session = *it;

        if (session == session_to_remove)
        {
            session->deleteLater();
            sessions->erase(it);
            return;
        }

        ++it;
    }
}

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
    catch (const std::error_code& error_code)
    {
        LOG(ERROR) << "Unable to get address for pending session:" << error_code;
    }

    return QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(const asio::ip::address& address,
                               quint16 port,
                               const std::chrono::minutes& idle_timeout,
                               bool statistics_enabled,
                               const std::chrono::seconds& statistics_interval,
                               QObject* parent)
    : QObject(parent),
      acceptor_(base::AsioEventDispatcher::currentIoContext()),
      address_(address),
      port_(port),
      idle_timeout_(idle_timeout),
      idle_timer_(new QTimer(this)),
      stat_timer_(new QTimer(this)),
      statistics_enabled_(statistics_enabled),
      statistics_interval_(statistics_interval)
{
    LOG(INFO) << "Ctor";

    connect(idle_timer_, &QTimer::timeout, this, &SessionManager::onIdleTimeout);
    connect(stat_timer_, &QTimer::timeout, this, &SessionManager::onStatTimeout);
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager()
{
    LOG(INFO) << "Dtor";

    std::error_code ignored_code;
    acceptor_.cancel(ignored_code);
    acceptor_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::start(std::shared_ptr<KeyPool> shared_key_pool)
{
    LOG(INFO) << "Starting session manager";

    asio::ip::tcp::endpoint endpoint(address_, port_);

    std::error_code error_code;
    acceptor_.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_.open failed:" << error_code;
        return;
    }

    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_.set_option failed:" << error_code;
        return;
    }

    acceptor_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_.bind failed:" << error_code;
        return;
    }

    acceptor_.listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_.listen failed:" << error_code;
        return;
    }

    start_time_ = Clock::now();

    shared_key_pool_ = std::move(shared_key_pool);
    DCHECK(shared_key_pool_);

    idle_timer_->start(kIdleTimerInterval);

    if (statistics_enabled_)
        stat_timer_->start(statistics_interval_);

    SessionManager::doAccept(this);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::disconnectSession(quint64 session_id)
{
    LOG(INFO) << "Disconnect session by session id:" << session_id;

    for (const auto& session : std::as_const(active_sessions_))
    {
        if (session->sessionId() == session_id)
        {
            session->disconnect();
            return;
        }
    }

    LOG(ERROR) << "Session with id" << session_id << "not found";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onPendingSessionReady(
    PendingSession* pending_session, const proto::relay::PeerToRelay& message)
{
    LOG(INFO) << "Pending session ready for key_id:" << message.key_id();

    // Looking for a key with the specified identifier.
    std::optional<KeyPool::Key> key = shared_key_pool_->key(message.key_id(), message.public_key());
    if (key.has_value())
    {
        // Decrypt the identifiers of peers.
        QByteArray secret = decryptSecret(message, *key);
        if (!secret.isEmpty())
        {
            // Save the identifiers of peers and the identifier of their shared key.
            pending_session->setIdentify(message.key_id(), secret);

            // Trying to find a peer that wants to be connected.
            for (const auto& other_pending_session : std::as_const(pending_sessions_))
            {
                if (pending_session->isPeerFor(*other_pending_session))
                {
                    LOG(INFO) << "Both peers are connected with key" << message.key_id();

                    // Delete the key from the pool. It can no longer be used.
                    shared_key_pool_->removeKey(message.key_id());

                    Session* session = new Session(
                        std::make_pair(pending_session->takeSocket(), other_pending_session->takeSocket()),
                        secret, this);

                    connect(session, &Session::sig_sessionFinished,
                            this, &SessionManager::onSessionFinished);

                    // Now the opposite peer is found, start the data transfer between them.
                    active_sessions_.emplace_back(session);
                    session->start();

                    emit sig_sessionStarted();

                    // Pending sessions are no longer needed, remove them.
                    removePendingSession(other_pending_session);
                    removePendingSession(pending_session);
                    return;
                }
            }

            LOG(INFO) << "Second peer has not connected yet";
            return;
        }
        else
        {
            LOG(ERROR) << "Failed to decrypt shared secret. Connection will be completed";
        }
    }
    else
    {
        LOG(ERROR) << "Key with id" << message.key_id() << "NOT found!";
    }

    // The key was not found in the pool.
    removePendingSession(pending_session);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onPendingSessionFailed(PendingSession* session)
{
    removePendingSession(session);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onSessionFinished(Session* session)
{
    removeSession(session);
}

//--------------------------------------------------------------------------------------------------
// static
void SessionManager::doAccept(SessionManager* self)
{
    self->acceptor_.async_accept(
        [self](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (!error_code)
        {
            LOG(INFO) << "New accepted connection:" << peerAddress(socket);

            PendingSession* session = new PendingSession(std::move(socket), self);

            connect(session, &PendingSession::sig_pendingSessionReady,
                    self, &SessionManager::onPendingSessionReady);
            connect(session, &PendingSession::sig_pendingSessionFailed,
                    self, &SessionManager::onPendingSessionFailed);

            // A new peer is connected. Create and start the pending session.
            self->pending_sessions_.emplace_back(session);
            session->start();
        }
        else
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(ERROR) << "Error while accepting connection:" << error_code;
        }

        // Waiting for the next connection.
        SessionManager::doAccept(self);
    });
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onIdleTimeout()
{
    auto current_time = Session::Clock::now();
    auto it = active_sessions_.begin();
    int count = 0;

    while (it != active_sessions_.end())
    {
        Session* session = *it;

        if (session->idleTime(current_time) >= idle_timeout_)
        {
            session->deleteLater();
            it = active_sessions_.erase(it);
            ++count;
        }
        else
        {
            ++it;
        }
    }

    LOG(INFO) << "Sessions ended by timeout:" << count;
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onStatTimeout()
{
    Session::TimePoint now = Session::Clock::now();

    proto::router::RelayStat relay_stat;
    relay_stat.set_uptime(
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count());

    for (const auto& session : std::as_const(active_sessions_))
    {
        proto::router::PeerConnection* peer_connection = relay_stat.add_peer_connection();

        peer_connection->set_session_id(session->sessionId());
        peer_connection->set_status(proto::router::PeerConnection::PEER_STATUS_ACTIVE);
        peer_connection->set_client_address(session->clientAddress().toStdString());
        peer_connection->set_client_user_name(session->clientUserName().toStdString());
        peer_connection->set_host_address(session->hostAddress().toStdString());
        peer_connection->set_host_id(session->hostId());
        peer_connection->set_bytes_transferred(session->bytesTransferred());
        peer_connection->set_idle_time(session->idleTime(now).count());
        peer_connection->set_duration(session->duration(now).count());
    }

    emit sig_sessionStatistics(relay_stat);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::removePendingSession(PendingSession* session)
{
    removeSessionT(&pending_sessions_, session);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::removeSession(Session* session)
{
    removeSessionT(&active_sessions_, session);
    emit sig_sessionFinished();
}

} // namespace relay
