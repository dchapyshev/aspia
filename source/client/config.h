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

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <QByteArray>
#include <QString>

#include <optional>

#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

namespace proto::control {
class Config;
} // namespace proto::control

// Every field is held as plain text: the key that would decrypt them sits in memory anyway for as
// long as the address book is unlocked, so keeping the ciphertext next to it protects nothing.
//
// What reaches the database is another matter. Whatever of a record has to be kept secret goes
// there as one column - serialized together, then encrypted - so no single field can be lifted out
// of it and stored where it would be read as something else. encryptedData() and its setter are
// that boundary, and they are the only place in the class that touches the cipher.
class RouterConfig final
{
public:
    RouterConfig();

    bool isValid() const;
    bool hasSameParams(const RouterConfig& other) const;

    // Returns user-set display name if non-empty, otherwise falls back to address.
    QString displayLabel() const;

    qint64 routerId() const { return router_id_; }
    void setRouterId(qint64 id) { router_id_ = id; }

    proto::router::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::router::SessionType type) { session_type_ = type; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    const QString& displayName() const { return display_name_; }
    void setDisplayName(const QString& value) { display_name_ = value; }

    const QString& address() const { return address_; }
    void setAddress(const QString& value) { address_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    // Bearer "remember this device" token issued by the router after a successful TOTP
    // submission. Empty until the user enrolls or enters a TOTP code at least once.
    const QByteArray& deviceToken() const { return device_token_; }
    void setDeviceToken(const QByteArray& value) { device_token_ = value; }

    // Clears the stored token. Called when the router rejects it (revoked remotely, password
    // changed elsewhere) so the next login walks the TOTP path again.
    void clearDeviceToken() { device_token_.clear(); }

    // The encrypted column for database I/O. Returns nothing when the record cannot be sealed, so a
    // failure is never written as a record that simply holds nothing. The setter answers whether the
    // column opened; on a refusal the fields it carries are left empty.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    qint64 router_id_ = -1;
    proto::router::SessionType session_type_;
    QString guid_;
    QString display_name_;
    QString address_;
    QString username_;
    SecureString password_;
    QByteArray device_token_;
};

class RouterHostConfig final
{
public:
    RouterHostConfig() = default;

    qint64 routerId() const { return router_id_; }
    void setRouterId(qint64 id) { router_id_ = id; }

    HostId hostId() const { return host_id_; }
    void setHostId(HostId id) { host_id_ = id; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    // The sealed column for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    qint64 router_id_ = -1;
    HostId host_id_ = kInvalidHostId;
    QString username_;
    SecureString password_;
};

class LocalHostConfig final
{
public:
    LocalHostConfig() = default;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 groupId() const { return group_id_; }
    void setGroupId(qint64 id) { group_id_ = id; }

    qint64 routerId() const { return router_id_; }
    void setRouterId(qint64 id) { router_id_ = id; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    qint64 createTime() const { return create_time_; }
    void setCreateTime(qint64 value) { create_time_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    qint64 connectTime() const { return connect_time_; }
    void setConnectTime(qint64 value) { connect_time_ = value; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const QString& comment() const { return comment_; }
    void setComment(const QString& value) { comment_ = value; }

    const QString& address() const { return address_; }
    void setAddress(const QString& value) { address_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    // The sealed column for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    qint64 id_ = -1;
    qint64 group_id_ = 0;
    qint64 router_id_ = 0;
    QString guid_;
    qint64 create_time_ = 0;
    qint64 modify_time_ = 0;
    qint64 connect_time_ = 0;
    QString name_;
    QString comment_;
    QString address_;
    QString username_;
    SecureString password_;
};

class LocalGroupConfig final
{
public:
    LocalGroupConfig() = default;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 parentId() const { return parent_id_; }
    void setParentId(qint64 id) { parent_id_ = id; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const QString& comment() const { return comment_; }
    void setComment(const QString& value) { comment_ = value; }

private:
    qint64 id_ = -1;
    qint64 parent_id_ = 0;
    QString name_;
    QString comment_;
};

class HostConfig final
{
public:
    HostConfig() = default;

    static HostConfig forLocalHost(const LocalHostConfig& host);
    static HostConfig forRouterHost(qint64 router_id, HostId host_id, const QString& name);

    qint64 routerId() const { return router_id_; }
    void setRouterId(qint64 id) { router_id_ = id; }

    const QString& address() const { return address_; }
    void setAddress(const QString& value) { address_ = value; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

private:
    qint64 router_id_ = 0;
    QString address_;
    QString name_;
    QString username_;
    SecureString password_;
};

proto::control::Config defaultDesktopConfig();

#endif // CLIENT_CONFIG_H
