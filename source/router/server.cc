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

#include "router/server.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/crypto/key_pair.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/net/network_channel.h"
#include "router/database_factory_sqlite.h"
#include "router/database_sqlite.h"
#include "router/session_admin.h"
#include "router/session_client.h"
#include "router/session_host.h"
#include "router/session_relay.h"
#include "router/settings.h"
#include "router/user_list_db.h"

namespace router {

namespace {

const char* sessionTypeToString(proto::RouterSession session_type)
{
    switch (session_type)
    {
        case proto::ROUTER_SESSION_CLIENT:
            return "ROUTER_SESSION_CLIENT";

        case proto::ROUTER_SESSION_HOST:
            return "ROUTER_SESSION_HOST";

        case proto::ROUTER_SESSION_ADMIN:
            return "ROUTER_SESSION_ADMIN";

        case proto::ROUTER_SESSION_RELAY:
            return "ROUTER_SESSION_RELAY";

        default:
            return "ROUTER_SESSION_UNKNOWN";
    }
}

} // namespace

Server::Server(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      database_factory_(std::make_shared<DatabaseFactorySqlite>())
{
    DCHECK(task_runner_);
}

Server::~Server() = default;

bool Server::start()
{
    if (server_)
        return false;

    std::unique_ptr<Database> database = database_factory_->openDatabase();
    if (!database)
    {
        LOG(LS_ERROR) << "Failed to open the database";
        return false;
    }

    Settings settings;

    base::ByteArray private_key = settings.privateKey();
    if (private_key.empty())
    {
        LOG(LS_INFO) << "The private key is not specified in the configuration file";

        base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
        if (!key_pair.isValid())
        {
            LOG(LS_ERROR) << "Failed to generate keys";
            return false;
        }

        std::filesystem::path public_key_path;
        if (!base::BasePaths::commonAppData(&public_key_path))
        {
            LOG(LS_ERROR) << "Failed to get the path to the current directory";
            return false;
        }

        public_key_path.append("aspia/router.pub");

        std::error_code error_code;
        if (std::filesystem::exists(public_key_path))
        {
            LOG(LS_INFO) << "A public key file exists and will be deleted: " << public_key_path;

            if (!std::filesystem::remove(public_key_path, error_code))
            {
                LOG(LS_ERROR) << "Failed to delete old public key: "
                              << base::utf16FromLocal8Bit(error_code.message());
                return false;
            }
        }

        std::string public_key = base::toHex(key_pair.publicKey());
        private_key = key_pair.privateKey();

        if (public_key.empty() || private_key.empty())
        {
            LOG(LS_ERROR) << "Empty keys generated";
            return false;
        }

        if (!base::writeFile(public_key_path, public_key))
        {
            LOG(LS_ERROR) << "Failed to write public key to file: " << public_key_path;
            return false;
        }
        else
        {
            LOG(LS_INFO) << "New public key saved to file: " << public_key_path;
        }

        settings.setPrivateKey(private_key);
        settings.flush();

        LOG(LS_INFO) << "New public key: " << public_key;
    }

    uint16_t port = settings.port();
    if (!port)
    {
        LOG(LS_ERROR) << "Invalid port specified in configuration file";
        return false;
    }

    std::unique_ptr<base::UserListBase> user_list = UserListDb::open(*database_factory_);

    authenticator_manager_ =
        std::make_unique<base::ServerAuthenticatorManager>(task_runner_, this);
    authenticator_manager_->setPrivateKey(private_key);
    authenticator_manager_->setUserList(std::move(user_list));
    authenticator_manager_->setAnonymousAccess(
        base::ServerAuthenticator::AnonymousAccess::ENABLE,
        proto::ROUTER_SESSION_HOST | proto::ROUTER_SESSION_RELAY);

    relay_key_pool_ = std::make_unique<SharedKeyPool>(this);

    server_ = std::make_unique<base::NetworkServer>();
    server_->start(port, this);

    LOG(LS_INFO) << "Server started";
    return true;
}

std::unique_ptr<proto::SessionList> Server::sessionList() const
{
    std::unique_ptr<proto::SessionList> result = std::make_unique<proto::SessionList>();

    for (const auto& session : sessions_)
    {
        proto::Session* item = result->add_session();

        item->set_session_id(session->sessionId());
        item->set_session_type(session->sessionType());
        item->set_timepoint(session->startTime());
        item->set_ip_address(session->address());
        item->mutable_version()->CopyFrom(session->version().toProto());
        item->set_os_name(session->osName());
        item->set_computer_name(session->computerName());

        switch (session->sessionType())
        {
            case proto::ROUTER_SESSION_HOST:
            {
                proto::HostSessionData session_data;

                for (const auto& host_id : static_cast<SessionHost*>(session.get())->hostIdList())
                    session_data.add_host_id(host_id);

                item->set_session_data(session_data.SerializeAsString());
            }
            break;

            case proto::ROUTER_SESSION_RELAY:
            {
                proto::RelaySessionData session_data;
                session_data.set_pool_size(relay_key_pool_->countForRelay(session->sessionId()));
                item->set_session_data(session_data.SerializeAsString());
            }
            break;

            default:
                break;
        }
    }

    result->set_error_code(proto::SessionList::SUCCESS);
    return result;
}

bool Server::stopSession(Session::SessionId session_id)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        if (it->get()->sessionId() == session_id)
        {
            sessions_.erase(it);
            return true;
        }
    }

    return false;
}

void Server::onHostSessionWithId(SessionHost* session)
{
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->get()->sessionType() == proto::ROUTER_SESSION_HOST)
        {
            SessionHost* other_session = reinterpret_cast<SessionHost*>(it->get());
            if (other_session != session)
            {
                for (const auto& host_id : session->hostIdList())
                {
                    if (other_session->hasHostId(host_id))
                    {
                        LOG(LS_INFO) << "Detected previous connection with ID " << host_id;

                        it = sessions_.erase(it);
                        continue;
                    }
                }
            }
        }

        ++it;
    }
}

SessionHost* Server::hostSessionById(base::HostId host_id)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        Session* entry = it->get();

        if (entry->sessionType() == proto::ROUTER_SESSION_HOST &&
            static_cast<SessionHost*>(entry)->hasHostId(host_id))
        {
            return static_cast<SessionHost*>(entry);
        }
    }

    return nullptr;
}

Session* Server::sessionById(Session::SessionId session_id)
{
    for (auto& session : sessions_)
    {
        if (session->sessionId() == session_id)
            return session.get();
    }

    return nullptr;
}

void Server::onNewConnection(std::unique_ptr<base::NetworkChannel> channel)
{
    LOG(LS_INFO) << "New connection: " << channel->peerAddress();

    channel->setOwnKeepAlive(true);
    channel->setNoDelay(true);

    if (authenticator_manager_)
        authenticator_manager_->addNewChannel(std::move(channel));
}

void Server::onPoolKeyUsed(Session::SessionId session_id, uint32_t key_id)
{
    for (const auto& session : sessions_)
    {
        SessionRelay* relay_session = static_cast<SessionRelay*>(session.get());
        if (relay_session->sessionId() == session_id)
            relay_session->sendKeyUsed(key_id);
    }
}

void Server::onNewSession(base::ServerAuthenticatorManager::SessionInfo&& session_info)
{
    std::string address = base::utf8FromUtf16(session_info.channel->peerAddress());
    proto::RouterSession session_type =
        static_cast<proto::RouterSession>(session_info.session_type);

    LOG(LS_INFO) << "New session: " << sessionTypeToString(session_type) << " (" << address << ")";

    std::unique_ptr<Session> session;

    switch (session_info.session_type)
    {
        case proto::ROUTER_SESSION_CLIENT:
            session = std::make_unique<SessionClient>();
            break;

        case proto::ROUTER_SESSION_HOST:
            session = std::make_unique<SessionHost>();
            break;

        case proto::ROUTER_SESSION_ADMIN:
            session = std::make_unique<SessionAdmin>();
            break;

        case proto::ROUTER_SESSION_RELAY:
            session = std::make_unique<SessionRelay>();
            break;

        default:
            break;
    }

    if (!session)
    {
        LOG(LS_ERROR) << "Unsupported session type: "
                      << static_cast<int>(session_info.session_type);
        return;
    }

    session->setChannel(std::move(session_info.channel));
    session->setDatabaseFactory(database_factory_);
    session->setServer(this);
    session->setRelayKeyPool(relay_key_pool_->share());
    session->setVersion(session_info.version);
    session->setOsName(session_info.os_name);
    session->setComputerName(session_info.computer_name);

    sessions_.emplace_back(std::move(session));
    sessions_.back()->start(this);
}

void Server::onSessionFinished(Session::SessionId session_id, proto::RouterSession /* session_type */)
{
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
    {
        if (it->get()->sessionId() == session_id)
        {
            // Session will be destroyed after completion of the current call.
            task_runner_->deleteSoon(std::move(*it));

            // Delete a session from the list.
            sessions_.erase(it);
            break;
        }
    }
}

} // namespace router
