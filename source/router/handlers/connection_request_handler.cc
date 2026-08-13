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

#include "router/handlers/connection_request_handler.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "proto/relay_peer.h"
#include "proto/router_constants.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"

//--------------------------------------------------------------------------------------------------
ConnectionRequestResult handleConnectionRequest(SharedHosts& hosts, SharedKeyPool& key_pool,
                                                const ConnectionRequestClient& client)
{
    ConnectionRequestResult result;

    const std::optional<SharedHosts::Host> host_info = hosts.find(client.host_id);
    if (!host_info.has_value())
    {
        LOG(ERROR) << "Host with id" << client.host_id << "NOT found!";
        result.offer.set_error_code(proto::router::kErrorHostOffline);
        return result;
    }

    // The key is consumed here: it is one-time, so a failure further down would waste it. Nothing
    // below can fail.
    std::optional<SharedKeyPool::Credentials> credentials = key_pool.take();
    if (!credentials.has_value())
    {
        LOG(ERROR) << "Empty key pool";
        result.offer.set_error_code(proto::router::kErrorKeyPoolEmpty);
        return result;
    }

    result.relay_session_id = credentials->session_id;
    result.relay_key_id = credentials->key.key_id();

    result.offer.set_error_code(proto::router::kErrorOk);

    proto::router::PeerInfo* peer_info = result.offer.mutable_peer_info();
    peer_info->set_is_legacy(host_info->version < kVersion_3_0_0);

    if (client.stun_port)
    {
        // An empty host string means that the client should use the router's address as the
        // server's host address. This is done to allow for future expansion, but is not currently
        // used.
        proto::router::StunServerInfo* stun_info = result.offer.mutable_stun_info();
        stun_info->set_version(1);
        stun_info->set_host("");
        stun_info->set_port(client.stun_port);
    }

    proto::router::RelayCredentials* offer_credentials = result.offer.mutable_relay();
    offer_credentials->set_host(credentials->peer_host);
    offer_credentials->set_port(credentials->peer_port);
    offer_credentials->mutable_key()->Swap(&credentials->key);

    // AES-256-GCM is used only when both peers support it; legacy peers (and their relay code)
    // only understand ChaCha20-Poly1305. The key material itself is algorithm-agnostic.
    const bool aes = host_info->version >= kVersion_3_0_0 && client.version >= kVersion_3_0_0;
    offer_credentials->mutable_key()->set_encryption(aes ?
        proto::router::RelayKey::ENCRYPTION_AES256_GCM :
        proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);

    // What the relay is told about the pair it is about to serve. Both peers hand it the same
    // blob, which is how the relay matches them; the addresses let it refuse a stranger that
    // presents a stolen key.
    proto::relay::PeerToRelay::Secret secret;
    secret.set_random_data(Random::string(16));
    secret.set_client_address(client.address);
    secret.set_client_user_name(client.user_name);
    secret.set_host_address(host_info->address);
    secret.set_host_id(client.host_id);

    offer_credentials->set_secret(secret.SerializeAsString());

    return result;
}
