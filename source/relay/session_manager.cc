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
#include "base/peer/host_id.h"
#include "base/strings/unicode.h"

namespace relay {

namespace {

const std::chrono::minutes kIdleTimerInterval { 1 };

// Decrypts an encrypted pair of peer identifiers using key |session_key|.
base::ByteArray decryptSecret(const proto::PeerToRelay& message, const SharedPool::Key& key)
{
    if (key.first.empty() || key.second.empty())
    {
        LOG(LS_ERROR) << "Invalid session key";
        return base::ByteArray();
    }

    std::unique_ptr<base::MessageDecryptor> decryptor =
        base::MessageDecryptorOpenssl::createForChaCha20Poly1305(key.first, key.second);
    if (!decryptor)
    {
        LOG(LS_ERROR) << "Decryptor not created";
        return base::ByteArray();
    }

    const std::string& source = message.data();
    if (source.empty())
    {
        LOG(LS_ERROR) << "Empty 'data' field";
        return base::ByteArray();
    }

    base::ByteArray target;
    target.resize(decryptor->decryptedDataSize(source.size()));

    if (!decryptor->decrypt(source.data(), source.size(), target.data()))
    {
        LOG(LS_ERROR) << "Failed to decrypt shared secret";
        return base::ByteArray();
    }

    return target;
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

SessionManager::SessionManager(std::shared_ptr<base::TaskRunner> task_runner,
                               uint16_t port,
                               const std::chrono::minutes& idle_timeout)
    : task_runner_(std::move(task_runner)),
      acceptor_(base::MessageLoop::current()->pumpAsio()->ioContext(),
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      idle_timeout_(idle_timeout),
      idle_timer_(base::MessageLoop::current()->pumpAsio()->ioContext())
{
    DCHECK(task_runner_);

    LOG(LS_INFO) << "Session manager port: " << port;
}

SessionManager::~SessionManager()
{
    std::error_code ignored_code;
    acceptor_.cancel(ignored_code);
    acceptor_.close(ignored_code);
    idle_timer_.cancel();
}

void SessionManager::start(std::unique_ptr<SharedPool> shared_pool, Delegate* delegate)
{
    LOG(LS_INFO) << "Starting session manager";

    shared_pool_ = std::move(shared_pool);
    delegate_ = delegate;

    DCHECK(delegate_ && shared_pool_);

    idle_timer_.expires_after(kIdleTimerInterval);
    idle_timer_.async_wait(std::bind(&SessionManager::doIdleTimeout, this, std::placeholders::_1));

    SessionManager::doAccept(this);
}

void SessionManager::onPendingSessionReady(
    PendingSession* session, const proto::PeerToRelay& message)
{
    LOG(LS_INFO) << "Pending session ready for key_id: " << message.key_id();

    // Looking for a key with the specified identifier.
    std::optional<SharedPool::Key> key = shared_pool_->key(message.key_id(), message.public_key());
    if (key.has_value())
    {
        // Decrypt the identifiers of peers.
        base::ByteArray secret = decryptSecret(message, *key);
        if (!secret.empty())
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
                        std::make_pair(session->takeSocket(), other_session->takeSocket())));
                    active_sessions_.back()->start(this);

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
            LOG(LS_WARNING) << "Failed to decrypt shared secret. Connection will be completed";
        }
    }
    else
    {
        LOG(LS_WARNING) << "Key with id " << message.key_id() << " NOT found!";
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
void SessionManager::doAccept(SessionManager* self)
{
    self->acceptor_.async_accept(
        [self](const std::error_code& error_code, asio::ip::tcp::socket socket)
    {
        if (!error_code)
        {
            LOG(LS_INFO) << "New accepted connection: " << base::utf16FromLocal8Bit(
                socket.remote_endpoint().address().to_string());

            // A new peer is connected. Create and start the pending session.
            self->pending_sessions_.emplace_back(std::make_unique<PendingSession>(
                self->task_runner_, std::move(socket), self));
            self->pending_sessions_.back()->start();
        }
        else
        {
            if (error_code == asio::error::operation_aborted)
                return;

            LOG(LS_ERROR) << "Error while accepting connection: "
                          << base::utf16FromLocal8Bit(error_code.message());
        }

        // Waiting for the next connection.
        SessionManager::doAccept(self);
    });
}

// static
void SessionManager::doIdleTimeout(SessionManager* self, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    self->doIdleTimeoutImpl(error_code);
}

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
        LOG(LS_ERROR) << "Error in idle timer: " << base::utf16FromLocal8Bit(error_code.message());
    }

    idle_timer_.expires_after(kIdleTimerInterval);
    idle_timer_.async_wait(std::bind(&SessionManager::doIdleTimeout, this, std::placeholders::_1));
}

void SessionManager::removePendingSession(PendingSession* session)
{
    task_runner_->deleteSoon(removeSessionT(&pending_sessions_, session));
}

void SessionManager::removeSession(Session* session)
{
    task_runner_->deleteSoon(removeSessionT(&active_sessions_, session));

    if (delegate_)
        delegate_->onSessionFinished();
}

} // namespace relay
