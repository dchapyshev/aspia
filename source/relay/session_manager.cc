//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "base/threading/asio_event_dispatcher.h"

namespace relay {

namespace {

const std::chrono::minutes kIdleTimerInterval { 1 };

//--------------------------------------------------------------------------------------------------
// Decrypts an encrypted pair of peer identifiers using key |session_key|.
QByteArray decryptSecret(const proto::PeerToRelay& message, const SharedPool::Key& key)
{
    if (key.first.isEmpty() || key.second.isEmpty())
    {
        LOG(LS_ERROR) << "Invalid session key";
        return QByteArray();
    }

    std::unique_ptr<base::MessageDecryptor> decryptor =
        base::MessageDecryptorOpenssl::createForChaCha20Poly1305(key.first, key.second);
    if (!decryptor)
    {
        LOG(LS_ERROR) << "Decryptor not created";
        return QByteArray();
    }

    const std::string& source = message.data();
    if (source.empty())
    {
        LOG(LS_ERROR) << "Empty 'data' field";
        return QByteArray();
    }

    QByteArray target;
    target.resize(static_cast<QByteArray::size_type>(decryptor->decryptedDataSize(source.size())));

    if (!decryptor->decrypt(source.data(), source.size(), target.data()))
    {
        LOG(LS_ERROR) << "Failed to decrypt shared secret";
        return QByteArray();
    }

    return target;
}

//--------------------------------------------------------------------------------------------------
// Removes a session from the list and returns a pointer to it.
template<class T>
std::unique_ptr<T> removeSessionT(std::vector<std::unique_ptr<T>>* session_list, T* session)
{
    session->stop();

    auto it = session_list->begin();
    while (it != session_list->end())
    {
        if (it->get() == session)
            break;

        ++it;
    }

    if (it != session_list->end())
    {
        std::unique_ptr<T> result = std::move(*it);
        session_list->erase(it);
        return result;
    }

    return nullptr;
}

QString peerAddress(const asio::ip::tcp::socket& socket)
{
    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket.remote_endpoint(error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Unable to get endpoint for accepted connection: " << error_code;
        }
        else
        {
            std::string address = endpoint.address().to_string();
            return QString::fromLocal8Bit(address.c_str(), static_cast<int>(address.size()));
        }
    }
    catch (const std::error_code& error_code)
    {
        LOG(LS_ERROR) << "Unable to get address for pending session: " << error_code;
    }

    return QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(std::shared_ptr<base::TaskRunner> task_runner,
                               const asio::ip::address& address,
                               quint16 port,
                               const std::chrono::minutes& idle_timeout,
                               bool statistics_enabled,
                               const std::chrono::seconds& statistics_interval,
                               QObject* parent)
    : QObject(parent),
      task_runner_(std::move(task_runner)),
      acceptor_(base::AsioEventDispatcher::currentIoContext()),
      address_(address),
      port_(port),
      idle_timeout_(idle_timeout),
      idle_timer_(base::AsioEventDispatcher::currentIoContext()),
      stat_timer_(base::AsioEventDispatcher::currentIoContext()),
      statistics_enabled_(statistics_enabled),
      statistics_interval_(statistics_interval)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_);
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager()
{
    LOG(LS_INFO) << "Dtor";

    std::error_code ignored_code;
    acceptor_.cancel(ignored_code);
    acceptor_.close(ignored_code);
    idle_timer_.cancel();
}

//--------------------------------------------------------------------------------------------------
void SessionManager::start(std::unique_ptr<SharedPool> shared_pool, Delegate* delegate)
{
    LOG(LS_INFO) << "Starting session manager";

    asio::ip::tcp::endpoint endpoint(address_, port_);

    std::error_code error_code;
    acceptor_.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_.open failed: " << error_code;
        return;
    }

    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_.set_option failed: " << error_code;
        return;
    }

    acceptor_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_.bind failed: " << error_code;
        return;
    }

    acceptor_.listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_.listen failed: " << error_code;
        return;
    }

    start_time_ = Clock::now();

    shared_pool_ = std::move(shared_pool);
    delegate_ = delegate;

    DCHECK(delegate_ && shared_pool_);

    idle_timer_.expires_after(kIdleTimerInterval);
    idle_timer_.async_wait(std::bind(&SessionManager::doIdleTimeout, this, std::placeholders::_1));

    if (statistics_enabled_)
    {
        stat_timer_.expires_after(statistics_interval_);
        stat_timer_.async_wait(std::bind(&SessionManager::doStatTimeout, this, std::placeholders::_1));
    }

    SessionManager::doAccept(this);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::disconnectSession(quint64 session_id)
{
    LOG(LS_INFO) << "Disconnect session by session id: " << session_id;

    for (const auto& session : active_sessions_)
    {
        if (session->sessionId() == session_id)
        {
            session->disconnect();
            return;
        }
    }

    LOG(LS_ERROR) << "Session with id " << session_id << " not found";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onPendingSessionReady(
    PendingSession* session, const proto::PeerToRelay& message)
{
    LOG(LS_INFO) << "Pending session ready for key_id: " << message.key_id();

    // Looking for a key with the specified identifier.
    std::optional<SharedPool::Key> key = shared_pool_->key(message.key_id(), message.public_key());
    if (key.has_value())
    {
        // Decrypt the identifiers of peers.
        QByteArray secret = decryptSecret(message, *key);
        if (!secret.isEmpty())
        {
            // Save the identifiers of peers and the identifier of their shared key.
            session->setIdentify(message.key_id(), secret);

            // Trying to find a peer that wants to be connected.
            for (auto& other_session : pending_sessions_)
            {
                if (session->isPeerFor(*other_session))
                {
                    LOG(LS_INFO) << "Both peers are connected with key " << message.key_id();

                    // Delete the key from the pool. It can no longer be used.
                    shared_pool_->removeKey(message.key_id());

                    // Now the opposite peer is found, start the data transfer between them.
                    active_sessions_.emplace_back(std::make_unique<Session>(
                        std::make_pair(session->takeSocket(), other_session->takeSocket()), secret));
                    active_sessions_.back()->start(this);

                    if (delegate_)
                        delegate_->onSessionStarted();

                    // Pending sessions are no longer needed, remove them.
                    removePendingSession(other_session.get());
                    removePendingSession(session);
                    return;
                }
            }

            LOG(LS_INFO) << "Second peer has not connected yet";
            return;
        }
        else
        {
            LOG(LS_ERROR) << "Failed to decrypt shared secret. Connection will be completed";
        }
    }
    else
    {
        LOG(LS_ERROR) << "Key with id " << message.key_id() << " NOT found!";
    }

    // The key was not found in the pool.
    removePendingSession(session);
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
            LOG(LS_INFO) << "New accepted connection: " << peerAddress(socket);

            // A new peer is connected. Create and start the pending session.
            self->pending_sessions_.emplace_back(std::make_unique<PendingSession>(
                std::move(socket), self));
            self->pending_sessions_.back()->start();
        }
        else
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(LS_ERROR) << "Error while accepting connection: " << error_code;
        }

        // Waiting for the next connection.
        SessionManager::doAccept(self);
    });
}

//--------------------------------------------------------------------------------------------------
// static
void SessionManager::doIdleTimeout(SessionManager* self, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
    {
        LOG(LS_ERROR) << "Operation aborted";
        return;
    }

    self->doIdleTimeoutImpl(error_code);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::doIdleTimeoutImpl(const std::error_code& error_code)
{
    if (!error_code)
    {
        auto current_time = Session::Clock::now();
        auto it = active_sessions_.begin();
        int count = 0;

        while (it != active_sessions_.end())
        {
            if ((*it)->idleTime(current_time) >= idle_timeout_)
            {
                it = active_sessions_.erase(it);
                ++count;
            }
            else
            {
                ++it;
            }
        }

        LOG(LS_INFO) << "Sessions ended by timeout: " << count;
    }
    else
    {
        LOG(LS_ERROR) << "Error in idle timer: " << error_code;
    }

    idle_timer_.expires_after(kIdleTimerInterval);
    idle_timer_.async_wait(std::bind(&SessionManager::doIdleTimeout, this, std::placeholders::_1));
}

//--------------------------------------------------------------------------------------------------
// static
void SessionManager::doStatTimeout(SessionManager* self, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
    {
        LOG(LS_ERROR) << "Operation aborted";
        return;
    }

    self->doStatTimeoutImpl(error_code);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::doStatTimeoutImpl(const std::error_code& error_code)
{
    if (!error_code)
    {
        collectAndSendStatistics();
    }
    else
    {
        LOG(LS_ERROR) << "Error in stat timer: " << error_code;
    }

    stat_timer_.expires_after(statistics_interval_);
    stat_timer_.async_wait(std::bind(&SessionManager::doStatTimeout, this, std::placeholders::_1));
}

//--------------------------------------------------------------------------------------------------
void SessionManager::collectAndSendStatistics()
{
    Session::TimePoint now = Session::Clock::now();

    proto::RelayStat relay_stat;
    relay_stat.set_uptime(
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count());

    for (const auto& session : active_sessions_)
    {
        proto::PeerConnection* peer_connection = relay_stat.add_peer_connection();

        peer_connection->set_session_id(session->sessionId());
        peer_connection->set_status(proto::PeerConnection::PEER_STATUS_ACTIVE);
        peer_connection->set_client_address(session->clientAddress());
        peer_connection->set_client_user_name(session->clientUserName());
        peer_connection->set_host_address(session->hostAddress());
        peer_connection->set_host_id(session->hostId());
        peer_connection->set_bytes_transferred(session->bytesTransferred());
        peer_connection->set_idle_time(session->idleTime(now).count());
        peer_connection->set_duration(session->duration(now).count());
    }

    if (delegate_)
        delegate_->onSessionStatistics(relay_stat);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::removePendingSession(PendingSession* session)
{
    task_runner_->deleteSoon(removeSessionT(&pending_sessions_, session));
}

//--------------------------------------------------------------------------------------------------
void SessionManager::removeSession(Session* session)
{
    task_runner_->deleteSoon(removeSessionT(&active_sessions_, session));

    if (delegate_)
        delegate_->onSessionFinished();
}

} // namespace relay
