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

#include "proxy/controller_manager.h"

#include "base/task_runner.h"
#include "crypto/generic_hash.h"
#include "crypto/message_decryptor_openssl.h"
#include "crypto/message_encryptor_openssl.h"
#include "crypto/key_pair.h"
#include "net/network_channel.h"
#include "proxy/session_manager.h"
#include "proxy/settings.h"
#include "proxy/shared_pool.h"

namespace proxy {

ControllerManager::ControllerManager(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      server_(std::make_unique<net::Server>()),
      shared_pool_(std::make_unique<SharedPool>())
{
    // Nothing
}

ControllerManager::~ControllerManager() = default;

bool ControllerManager::start()
{
    Settings settings;

    crypto::KeyPair key_pair = crypto::KeyPair::fromPrivateKey(settings.proxyPrivateKey());
    if (!key_pair.isValid())
        return false;

    base::ByteArray temp = key_pair.sessionKey(settings.controllerPublicKey());
    if (temp.empty())
        return false;

    session_key_ = crypto::GenericHash::hash(crypto::GenericHash::Type::BLAKE2s256, temp);
    if (session_key_.empty())
        return false;

    session_manager_ = std::make_unique<SessionManager>(task_runner_, settings.peerPort());
    session_manager_->start(shared_pool_->share());

    server_->start(settings.controllerPort(), this);
    return true;
}

void ControllerManager::onNewConnection(std::unique_ptr<net::Channel> channel)
{
    base::ByteArray iv(12);
    std::fill(iv.begin(), iv.end(), 0);

    std::unique_ptr<crypto::MessageEncryptor> encryptor =
        crypto::MessageEncryptorOpenssl::createForChaCha20Poly1305(session_key_, iv);
    std::unique_ptr<crypto::MessageDecryptor> decryptor =
        crypto::MessageDecryptorOpenssl::createForChaCha20Poly1305(session_key_, iv);

    if (!encryptor || !decryptor)
        return;

    channel->setEncryptor(std::move(encryptor));
    channel->setDecryptor(std::move(decryptor));

    controllers_.emplace_back(
        std::make_unique<Controller>(shared_pool_->share(), std::move(channel), this));
    controllers_.back()->start();
}

void ControllerManager::onControllerFinished(Controller* controller)
{
    controller->stop();

    auto it = controllers_.begin();

    while (it != controllers_.end())
    {
        if (it->get() == controller)
            break;

        ++it;
    }

    if (it != controllers_.end())
    {
        task_runner_->deleteSoon(std::move(*it));
        controllers_.erase(it);
    }
}

} // namespace proxy
