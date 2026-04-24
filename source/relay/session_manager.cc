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

#include <QTimer>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/crypto/stream_decryptor.h"
#include "relay/pending_session.h"
#include "relay/session.h"
#include "relay/settings.h"

namespace relay {

namespace {

const std::chrono::minutes kIdleTimerInterval { 1 };

//--------------------------------------------------------------------------------------------------
// Decrypts an encrypted pair of peer identifiers using key |session_key|.
QByteArray decryptSecret(const proto::relay::PeerToRelay& message, const SessionManager::Key& key)
{
    if (key.first.isEmpty() || key.second.isEmpty())
    {
        LOG(ERROR) << "Invalid session key";
        return QByteArray();
    }

    std::unique_ptr<base::StreamDecryptor> decryptor =
        base::StreamDecryptor::createForChaCha20Poly1305(key.first, key.second);
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
    catch (const std::error_code& error_code)
    {
        LOG(ERROR) << "Unable to get address for pending session:" << error_code;
    }

    return QString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
SessionManager::SessionManager(QObject* parent)
    : QObject(parent),
      acceptor_(base::AsioEventDispatcher::ioContext()),
      idle_timer_(new QTimer(this)),
      stat_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    connect(idle_timer_, &QTimer::timeout, this, &SessionManager::onIdleTimeout);
    connect(stat_timer_, &QTimer::timeout, this, &SessionManager::onStatTimeout);
}

//--------------------------------------------------------------------------------------------------
SessionManager::~SessionManager()
{
    LOG(INFO) << "Dtor";

    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;

    std::error_code ignored_code;
    acceptor_.cancel(ignored_code);
    acceptor_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
bool SessionManager::start()
{
    LOG(INFO) << "Starting session manager";

    Settings settings;

    idle_timeout_ = settings.peerIdleTimeout();
    if (idle_timeout_ < std::chrono::minutes(1) || idle_timeout_ > std::chrono::minutes(60))
    {
        LOG(ERROR) << "Invalid peer idle specified";
        return false;
    }

    if (settings.isStatisticsEnabled())
    {
        std::chrono::seconds interval = settings.statisticsInterval();
        if (interval < std::chrono::seconds(1) || interval > std::chrono::minutes(60))
        {
            LOG(ERROR) << "Invalid statistics interval";
            return false;
        }
    }

    LOG(INFO) << "Peer idle timeout:" << idle_timeout_.count();
    LOG(INFO) << "Statistics enabled:" << settings.isStatisticsEnabled();
    LOG(INFO) << "Statistics interval:" << settings.statisticsInterval().count();

    qint16 port = settings.peerPort();
    if (port == 0)
    {
        LOG(ERROR) << "Invalid peer port";
        return false;
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
            return false;
        }
    }
    else
    {
        address = asio::ip::address_v6::any();
    }

    LOG(INFO) << "Listen interface:" << (iface.isEmpty() ? "ANY" : iface) << ":" << port;

    asio::ip::tcp::endpoint endpoint(address, port);

    std::error_code error_code;
    acceptor_.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "open failed:" << error_code;
        return false;
    }

    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), error_code);
    if (error_code)
    {
        LOG(ERROR) << "set_option failed:" << error_code;
        return false;
    }

    acceptor_.bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "bind failed:" << error_code;
        return false;
    }

    acceptor_.listen(asio::ip::tcp::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(ERROR) << "listen failed:" << error_code;
        return false;
    }

    start_time_ = Clock::now();
    idle_timer_->start(kIdleTimerInterval);

    if (settings.isStatisticsEnabled())
        stat_timer_->start(settings.statisticsInterval());

    SessionManager::doAccept(this);
    return true;
}

//--------------------------------------------------------------------------------------------------
quint32 SessionManager::addKey(SessionKey&& session_key)
{
    quint32 key_id = key_counter_++;
    key_pool_.try_emplace(key_id, std::move(session_key));

    LOG(INFO) << "Key with id" << key_id << "added to pool";
    return key_id;
}

//--------------------------------------------------------------------------------------------------
bool SessionManager::removeKey(quint32 key_id)
{
    auto result = key_pool_.find(key_id);
    if (result == key_pool_.end())
        return false;

    key_pool_.erase(result);
    LOG(INFO) << "Key with id" << key_id << "removed from pool";
    return true;
}

//--------------------------------------------------------------------------------------------------
void SessionManager::setKeyExpired(quint32 key_id)
{
    if (!removeKey(key_id))
        return;

    LOG(INFO) << "Key with ID" << key_id << "expired. It has been removed";
    emit sig_keyExpired(key_id);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::clearKeys()
{
    LOG(INFO) << "Key pool cleared";
    key_pool_.clear();
}

//--------------------------------------------------------------------------------------------------
std::optional<SessionManager::Key> SessionManager::keyFromPool(
    quint32 key_id, const std::string& peer_public_key) const
{
    auto result = key_pool_.find(key_id);
    if (result == key_pool_.end())
        return std::nullopt;

    const SessionKey& session_key = result->second;
    return std::make_pair(session_key.sessionKey(peer_public_key), session_key.iv());
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onDisconnectSession(quint64 session_id)
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
void SessionManager::onPendingSessionReady(const proto::relay::PeerToRelay& message)
{
    LOG(INFO) << "Pending session ready for key_id:" << message.key_id();

    PendingSession* pending_session = dynamic_cast<PendingSession*>(sender());
    CHECK(pending_session);

    // Looking for a key with the specified identifier.
    std::optional<Key> key = keyFromPool(message.key_id(), message.public_key());
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
        removeKey(message.key_id());

        Session* session = new Session(
            std::make_pair(pending_session->takeSocket(), other_pending_session->takeSocket()),
            secret, this);
        active_sessions_.emplace_back(session);

        connect(session, &Session::sig_finished, this, &SessionManager::onSessionFinished);

        // Now the opposite peer is found, start the data transfer between them.
        session->start();
        emit sig_started();

        // Pending sessions are no longer needed, remove them.
        removePendingSession(other_pending_session);
        removePendingSession(pending_session);
        return;
    }

    LOG(INFO) << "Second peer has not connected yet";
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onPendingSessionFailed()
{
    PendingSession* session = dynamic_cast<PendingSession*>(sender());
    CHECK(session);
    removePendingSession(session);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::onSessionFinished()
{
    Session* session = dynamic_cast<Session*>(sender());
    CHECK(session);
    removeSession(session);
}

//--------------------------------------------------------------------------------------------------
// static
void SessionManager::doAccept(SessionManager* self)
{
    auto guard = self->alive_guard_;
    self->acceptor_.async_accept([self, guard](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (!*guard)
            return;

        if (!error_code)
        {
            LOG(INFO) << "New accepted connection:" << peerAddress(socket);

            PendingSession* session = new PendingSession(std::move(socket), self);
            self->pending_sessions_.emplace_back(session);

            connect(session, &PendingSession::sig_ready, self, &SessionManager::onPendingSessionReady);
            connect(session, &PendingSession::sig_failed, self, &SessionManager::onPendingSessionFailed);

            // A new peer is connected. Create and start the pending session.
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
    int count = 0;

    for (auto it = active_sessions_.begin(); it != active_sessions_.end();)
    {
        Session* session = *it;

        if (session->idleTime(current_time) >= idle_timeout_)
        {
            session->disconnect();
            session->deleteLater();

            it = active_sessions_.erase(it);
            emit sig_finished();
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

    proto::router::RelayStatistics statistics;
    statistics.set_uptime(std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count());

    for (const auto& session : std::as_const(active_sessions_))
    {
        proto::router::Peer* peer = statistics.add_peer();
        peer->set_session_id(session->sessionId());
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

//--------------------------------------------------------------------------------------------------
void SessionManager::removePendingSession(PendingSession* session)
{
    session->disconnect();
    session->deleteLater();
    pending_sessions_.removeOne(session);
}

//--------------------------------------------------------------------------------------------------
void SessionManager::removeSession(Session* session)
{
    session->disconnect();
    session->deleteLater();
    active_sessions_.removeOne(session);
    emit sig_finished();
}

} // namespace relay
