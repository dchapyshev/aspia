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

#ifndef ROUTER_ROUTER_TEST_BASE_H
#define ROUTER_ROUTER_TEST_BASE_H

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <set>
#include <string>

#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "router/database.h"
#include "router/workspace.h"
#include "router/handlers/request_caller.h"

// Shared fixture for the request handlers: an isolated database in a temporary directory, starting
// from the state --create-config leaves behind (the built-in administrator with id 1), plus the
// building blocks every handler test needs - users, workspaces, hosts and groups.
class RouterTestBase : public testing::Test
{
protected:
    static constexpr quint32 kAllSessions = proto::router::SESSION_TYPE_ADMIN |
        proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_CLIENT;

    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/router.db3";
        ASSERT_TRUE(db_.open(file_path_));

        ASSERT_EQ(db_.addUser(makeUser("admin", kAllSessions)),
                  proto::router::kErrorOk);
        ASSERT_EQ(db_.findUser("admin", &admin_), proto::router::kErrorOk);
        ASSERT_EQ(admin_.entry_id, 1);

        caller_.user_id = admin_.entry_id;
        caller_.name = admin_.name.toStdString();
    }

    static RouterUser makeUser(const QString& name, quint32 sessions)
    {
        RouterUser user = RouterUser::create(name, SecureString("Password1234!"));
        user.sessions = sessions;
        user.flags = User::ENABLED;
        return user;
    }

    static std::string toStdString(const QByteArray& bytes)
    {
        return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    // Adds a user of the given session mask and returns the stored record.
    RouterUser addUser(const QString& name, quint32 sessions)
    {
        if (db_.addUser(makeUser(name, sessions)) != proto::router::kErrorOk)
            return RouterUser();
        return findUser(name);
    }

    // The stored record of a user, empty when there is none.
    RouterUser findUser(const QString& name)
    {
        RouterUser user;
        db_.findUser(name, &user);
        return user;
    }

    RouterUser findUser(qint64 entry_id)
    {
        RouterUser user;
        db_.findUser(entry_id, &user);
        return user;
    }

    // A session of the given user and type, in place of the built-in administrator.
    void setCaller(const RouterUser& user, proto::router::SessionType session_type)
    {
        caller_.user_id = user.entry_id;
        caller_.name = user.name.toStdString();
        caller_.session_type = session_type;
    }

    // A workspace the built-in administrator is a member of.
    qint64 addWorkspace(const QString& name)
    {
        qint64 entry_id = -1;
        if (db_.addWorkspace(name.toStdString(), std::string_view(), {admin_.entry_id},
                             &entry_id) != proto::router::kErrorOk)
        {
            return -1;
        }
        return entry_id;
    }

    // Moves a host to the given workspace and group, keeping the fields an operator edits.
    // workspace_id == 0 releases the host.
    std::string_view moveHost(HostId host_id, qint64 workspace_id, qint64 group_id = 0)
    {
        const proto::router::Host host = findHost(host_id);
        return db_.modifyHost(host_id, host.revision(), workspace_id, group_id,
                              host.display_name(), host.comment());
    }

    // An approved host, as the host worker creates it at first connection.
    HostId addHost(std::string_view key_hash)
    {
        if (!db_.addHost(key_hash, "hwid"))
            return kInvalidHostId;

        HostId host_id = kInvalidHostId;
        if (db_.hostId(key_hash, &host_id) != proto::router::kErrorOk)
            return kInvalidHostId;
        return host_id;
    }

    proto::router::Host findHost(HostId host_id)
    {
        proto::router::HostList list;
        db_.hosts(0, proto::router::kMaxHostPageSize, &list);

        for (int i = 0; i < list.host_size(); ++i)
        {
            if (list.host(i).host_id() == host_id)
                return list.host(i);
        }
        return proto::router::Host();
    }

    // Doctors rows the public API deliberately cannot produce (aged timestamps, induced holes).
    // A second connection to the same file is fine: the database runs in WAL mode.
    bool execRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return false;
        return raw.exec(sql.toStdString().c_str());
    }

    // Counts rows the public API deliberately hides, so a test can tell "not shown" from "not
    // there". Returns -1 when the query itself fails.
    qint64 countRaw(const QString& sql)
    {
        SqlDatabase raw;
        if (!raw.open(file_path_))
            return -1;

        SqlQuery query(raw, sql.toStdString().c_str());
        if (query.next() != SqlQuery::StepResult::ROW)
            return -1;

        return query.columnInt64(0);
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
    Database db_;
    RouterUser admin_;
    RequestCaller caller_;
};

#endif // ROUTER_ROUTER_TEST_BASE_H
