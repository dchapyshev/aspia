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

#include "base/crc32.h"
#include "base/task_runner.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "base/crypto/message_encryptor_openssl.h"
#include "base/crypto/key_pair.h"
#include "proxy/session_manager.h"
#include "proxy/settings.h"
#include "proxy/shared_pool.h"

namespace proxy {

namespace {

const uint32_t kEncryptorSeedNumber = 0xAF129900;
const uint32_t kDecryptorSeedNumber = 0x6712AF05;

base::ByteArray createIv(uint32_t seed, const base::ByteArray& session_key)
{
    static const size_t kIvSize = 12;

    base::ByteArray iv;
    iv.resize(kIvSize);

    uint32_t* data = reinterpret_cast<uint32_t*>(iv.data());

    for (size_t i = 0; i < kIvSize / sizeof(uint32_t); ++i)
        data[i] = seed = base::crc32(seed, session_key.data(), session_key.size());

    return iv;
}

} // namespace

ControllerManager::ControllerManager(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      server_(std::make_unique<base::NetworkServer>()),
      shared_pool_(std::make_unique<SharedPool>())
{
    // Nothing
}

ControllerManager::~ControllerManager() = default;

bool ControllerManager::start()
{
    Settings settings;

    base::KeyPair key_pair = base::KeyPair::fromPrivateKey(settings.proxyPrivateKey());
    if (!key_pair.isValid())
        return false;

    base::ByteArray temp = key_pair.sessionKey(settings.controllerPublicKey());
    if (temp.empty())
        return false;

    session_key_ = base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, temp);
    if (session_key_.empty())
        return false;

    session_manager_ = std::make_unique<SessionManager>(task_runner_, settings.peerPort());
    session_manager_->start(shared_pool_->share());

    server_->start(settings.controllerPort(), this);
    return true;
}

void ControllerManager::onNewConnection(std::unique_ptr<base::NetworkChannel> channel)
{
    std::unique_ptr<base::MessageEncryptor> encryptor =
        base::MessageEncryptorOpenssl::createForChaCha20Poly1305(
            session_key_, createIv(kEncryptorSeedNumber, session_key_));
    if (!encryptor)
        return;

    std::unique_ptr<base::MessageDecryptor> decryptor =
        base::MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key_, createIv(kDecryptorSeedNumber, session_key_));
    if (!decryptor)
        return;

    channel->setEncryptor(std::move(encryptor));
    channel->setDecryptor(std::move(decryptor));

    controllers_.emplace_back(std::make_unique<Controller>(
        current_controller_++, shared_pool_->share(), std::move(channel), this));
    controllers_.back()->start();
}

void ControllerManager::onControllerFinished(Controller* controller)
{
    shared_pool_->removeKeysForController(controller->id());
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
