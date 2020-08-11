//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "peer/peer_id.h"

#include <optional>

namespace relay {

namespace {

// Decrypts an encrypted pair of peer identifiers using key |session_key|.
std::optional<peer::PeerIdPair> decryptIdPair(
    const proto::PeerToRelay& message, const SessionKey& session_key)
{
    std::unique_ptr<base::MessageDecryptor> decryptor =
        base::MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key.sessionKey(message.public_key()), session_key.iv());
    if (!decryptor)
        return std::nullopt;

    const std::string& source = message.data();
    if (source.empty())
        return std::nullopt;

    std::string target;
    target.resize(decryptor->decryptedDataSize(source.size()));

    if (!decryptor->decrypt(source.data(), source.size(), target.data()))
        return std::nullopt;

    proto::PeerToRelay::IdPair id_pair;
    if (!id_pair.ParseFromString(target))
        return std::nullopt;

    return std::make_pair(id_pair.first(), id_pair.second());
}

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

} // namespace

SessionManager::SessionManager(std::shared_ptr<base::TaskRunner> task_runner, uint16_t port)
    : task_runner_(std::move(task_runner)),
      acceptor_(base::MessageLoop::current()->pumpAsio()->ioContext(),
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
{
    DCHECK(task_runner_);
}

SessionManager::~SessionManager()
{
    std::error_code ignored_code;
    acceptor_.cancel(ignored_code);
    acceptor_.close(ignored_code);
}

void SessionManager::start(std::unique_ptr<SharedPool> shared_pool)
{
    shared_pool_ = std::move(shared_pool);
    SessionManager::doAccept(this);
}

void SessionManager::onPendingSessionReady(
    PendingSession* session, const proto::PeerToRelay& message)
{
    // Looking for a key with the specified identifier.
    const SessionKey& session_key = shared_pool_->key(message.key_id());
    if (session_key.isValid())
    {
        // Decrypt the identifiers of peers.
        std::optional<peer::PeerIdPair> id_pair = decryptIdPair(message, session_key);
        if (id_pair.has_value())
        {
            // Save the identifiers of peers and the identifier of their shared key.
            session->setIdentify(id_pair.value(), message.key_id());

            // Trying to find a peer that wants to be connected.
            for (auto& other_session : pending_sessions_)
            {
                if (session->isPeerFor(*other_session))
                {
                    // Delete the key from the pool. It can no longer be used.
                    shared_pool_->removeKey(message.key_id());

                    // Now the opposite peer is found, start the data transfer between them.
                    active_sessions_.emplace_back(std::make_unique<Session>(
                        std::make_pair(session->takeSocket(), other_session->takeSocket())));
                    active_sessions_.back()->start(this);

                    // Pending sessions are no longer needed, remove them.
                    removePendingSession(other_session.get());
                    removePendingSession(session);
                    return;
                }
            }

            return;
        }
    }

    // The key was not found in the pool.
    removePendingSession(session);
}

void SessionManager::onPendingSessionFailed(PendingSession* session)
{
    removePendingSession(session);
}

void SessionManager::onSessionFinished(Session* session)
{
    removeSession(session);
}

// static
void SessionManager::doAccept(SessionManager* session_manager)
{
    session_manager->acceptor_.async_accept(
        [session_manager](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (error_code)
            return;

        // A new peer is connected. Create and start the pending session.
        session_manager->pending_sessions_.emplace_back(std::make_unique<PendingSession>(
            session_manager->task_runner_, std::move(socket), session_manager));
        session_manager->pending_sessions_.back()->start();

        // Waiting for the next connection.
        SessionManager::doAccept(session_manager);
    });
}

void SessionManager::removePendingSession(PendingSession* session)
{
    task_runner_->deleteSoon(removeSessionT(&pending_sessions_, session));
}

void SessionManager::removeSession(Session* session)
{
    task_runner_->deleteSoon(removeSessionT(&active_sessions_, session));
}

} // namespace relay
