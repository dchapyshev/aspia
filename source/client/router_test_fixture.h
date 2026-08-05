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

#ifndef CLIENT_ROUTER_TEST_FIXTURE_H
#define CLIENT_ROUTER_TEST_FIXTURE_H

#include <QHash>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "client/router_keys.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

// The account and the group keys a router session runs on, built for real: the crypto is part of
// the contract under test, so nothing is stubbed. Shared by the tests of the keys, of the codec
// and of the session state.
class RouterKeysFixture : public testing::Test
{
protected:
    static constexpr char kUserName[] = "admin";
    static constexpr char kPassword[] = "Password1234!";
    static constexpr qint64 kUserId = 1;

    void SetUp() override
    {
        user_ = RouterUser::create(QString::fromUtf8(kUserName), SecureString(kPassword));
        ASSERT_TRUE(user_.isValid());
    }

    // The group key of a workspace, invented on first use so every test shares one per id.
    const SecureByteArray& groupKey(qint64 workspace_id)
    {
        if (!group_keys_.contains(workspace_id))
            group_keys_.insert(workspace_id, SecureByteArray(Random::byteArray(32)));
        return group_keys_[workspace_id];
    }

    // The UserKeys message the router sends right after the two-factor stage.
    proto::router::UserKeys userKeys(const QList<qint64>& workspace_ids)
    {
        proto::router::UserKeys keys;
        keys.set_user_id(kUserId);
        keys.set_name(kUserName);
        keys.set_public_key(user_.public_key.toStdString());
        keys.set_wrap_private_key(user_.wrap_private_key.toStdString());
        keys.set_wrap_salt(user_.wrap_salt.toStdString());

        for (qint64 workspace_id : workspace_ids)
        {
            proto::router::UserKeys::WorkspaceKey* key = keys.add_workspace_key();
            key->set_workspace_id(workspace_id);
            key->set_wrapped_gk(
                SealedBox::seal(groupKey(workspace_id), user_.public_key).toStdString());
        }

        return keys;
    }

    // Loads the identity and the keys of the given workspaces into |keys|.
    void loadKeys(RouterKeys* keys, const QList<qint64>& workspace_ids)
    {
        ASSERT_EQ(keys->apply(userKeys(workspace_ids), SecureString(kPassword)),
                  RouterKeys::Result::OK);
    }

    std::string encrypt(qint64 workspace_id, const QString& plaintext)
    {
        const DataCryptor cryptor(CipherType::AES256_GCM, groupKey(workspace_id));
        std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext.toUtf8());
        if (!encrypted.has_value())
            return std::string();
        return encrypted->toStdString();
    }

    proto::router::HostList hostList(qint64 workspace_id, const QList<HostId>& host_ids,
                                     qint64 total_count)
    {
        proto::router::HostList list;
        list.set_error_code(proto::router::kErrorOk);
        list.set_workspace_id(workspace_id);
        list.set_total_count(total_count);

        for (HostId host_id : host_ids)
        {
            proto::router::Host* host = list.add_host();
            host->set_host_id(host_id);
            host->set_workspace_id(workspace_id);
            host->set_display_name("host");
        }

        return list;
    }

    RouterUser user_;
    QHash<qint64, SecureByteArray> group_keys_;
};

#endif // CLIENT_ROUTER_TEST_FIXTURE_H
