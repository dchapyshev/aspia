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

#include "relay/controller.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_common.pb.h"
#include "relay/settings.h"

#if defined(OS_WIN)
#include "base/files/base_paths.h"
#include "base/net/firewall_manager.h"
#endif // defined(OS_WIN)

namespace relay {

namespace {

const std::chrono::seconds kReconnectTimeout{ 30 };

#if defined(OS_WIN)
const wchar_t kFirewallRuleName[] = L"Aspia Relay Service";
const wchar_t kFirewallRuleDecription[] = L"Allow incoming TCP connections";
#endif // defined(OS_WIN)

} // namespace

Controller::Controller(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(task_runner),
      reconnect_timer_(task_runner),
      shared_pool_(std::make_unique<SharedPool>())
{
    Settings settings;

    // Router settings.
    router_address_ = settings.routerAddress();
    router_port_ = settings.routerPort();
    router_public_key_ = settings.routerPublicKey();

    LOG(LS_INFO) << "Router address: " << router_address_;
    LOG(LS_INFO) << "Router port: " << router_port_;
    LOG(LS_INFO) << "Router public key: " << base::toHex(router_public_key_);

    // Peers settings.
    peer_port_ = settings.peerPort();
    max_peer_count_ = settings.maxPeerCount();

    LOG(LS_INFO) << "Peer port: " << peer_port_;
    LOG(LS_INFO) << "Max peer count: " << max_peer_count_;
}

Controller::~Controller()
{
#if defined(OS_WIN)
    deleteFirewallRules();
#endif // defined(OS_WIN)
}

bool Controller::start()
{
    LOG(LS_INFO) << "Starting controller";

    if (router_address_.empty())
    {
        LOG(LS_ERROR) << "Empty router address";
        return false;
    }

    if (router_port_ == 0)
    {
        LOG(LS_ERROR) << "Invalid router port";
        return false;
    }

    if (router_public_key_.empty())
    {
        LOG(LS_ERROR) << "Empty router public key";
        return false;
    }

    if (peer_port_ == 0)
    {
        LOG(LS_ERROR) << "Invalid peer port";
        return false;
    }

#if defined(OS_WIN)
    addFirewallRules(peer_port_);
#endif // defined(OS_WIN)

    session_manager_ = std::make_unique<SessionManager>(task_runner_, peer_port_);
    session_manager_->start(shared_pool_->share(), this);

    connectToRouter();
    return true;
}

void Controller::onConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    static const std::chrono::seconds kKeepAliveTime{ 30 };
    static const std::chrono::seconds kKeepAliveInterval{ 3 };

    channel_->setKeepAlive(true, kKeepAliveTime, kKeepAliveInterval);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>(task_runner_);

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(router_public_key_);
    authenticator_->setSessionType(proto::ROUTER_SESSION_RELAY);

    authenticator_->start(std::move(channel_),
                          [this](base::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == base::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            LOG(LS_INFO) << "Authentication complete";

            // Now the session will receive incoming messages.
            channel_->resume();

            sendKeyPool(max_peer_count_);
        }
        else
        {
            LOG(LS_WARNING) << "Authentication failed: "
                            << base::ClientAuthenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        task_runner_->deleteSoon(std::move(authenticator_));
    });
}

void Controller::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "The connection to the router has been lost: "
                 << base::NetworkChannel::errorToString(error_code);

    // Clearing the key pool.
    shared_pool_->clear();

    // Retrying a connection at a time interval.
    delayedConnectToRouter();
}

void Controller::onMessageReceived(const base::ByteArray& /* buffer */)
{
    // Nothing
}

void Controller::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void Controller::onSessionFinished()
{
    // After disconnecting the peer, one key is released.
    // Send one new key to the router.
    sendKeyPool(1);
}

void Controller::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    // Create channel.
    channel_ = std::make_unique<base::NetworkChannel>();

    // Connect to router.
    channel_->setListener(this);
    channel_->connect(router_address_, router_port_);
}

void Controller::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_.start(kReconnectTimeout, std::bind(&Controller::connectToRouter, this));
}

void Controller::sendKeyPool(uint32_t key_count)
{
    proto::RelayToRouter message;
    proto::RelayKeyPool* relay_key_pool = message.mutable_key_pool();

    relay_key_pool->set_peer_port(peer_port_);

    // Add the requested number of keys to the pool.
    for (uint32_t i = 0; i < key_count; ++i)
    {
        SessionKey session_key = SessionKey::create();
        if (!session_key.isValid())
            return;

        // Add the key to the outgoing message.
        proto::RelayKey* key = relay_key_pool->add_key();

        key->set_type(proto::RelayKey::TYPE_X25519);
        key->set_encryption(proto::RelayKey::ENCRYPTION_CHACHA20_POLY1305);
        key->set_public_key(base::toStdString(session_key.publicKey()));
        key->set_iv(base::toStdString(session_key.iv()));

        // Add the key to the pool.
        key->set_key_id(shared_pool_->addKey(std::move(session_key)));
    }

    // Send a message to the router.
    channel_->send(base::serialize(message));
}

#if defined(OS_WIN)
void Controller::addFirewallRules(uint16_t port)
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, port))
        return;

    LOG(LS_INFO) << "Rule is added to the firewall";
}

void Controller::deleteFirewallRules()
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    firewall.deleteRuleByName(kFirewallRuleName);
}
#endif // defined(OS_WIN)

} // namespace relay
