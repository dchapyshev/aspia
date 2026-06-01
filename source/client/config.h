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

#include "base/crypto/secure_string.h"

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

namespace proto::control {
class Config;
} // namespace proto::control

// Sensitive text/binary fields are stored as ciphertext (QByteArray) and only decrypted on
// access through the public getters. Database I/O goes through the encryptedXxx() accessors
// to avoid a redundant decrypt+encrypt roundtrip.
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

    QString displayName() const;
    void setDisplayName(const QString& value);

    QString address() const;
    void setAddress(const QString& value);

    QString username() const;
    void setUsername(const QString& value);

    SecureString password() const;
    void setPassword(const SecureString& value);

    // Bearer "remember this device" token issued by the router after a successful TOTP
    // submission. Empty until the user enrolls or enters a TOTP code at least once. The
    // value is stored under a double wrap (OS keystore + master-password-derived key) so a
    // copy of |client.db3| moved to another machine cannot present a usable token even if
    // the master password is known.
    QByteArray deviceToken() const;
    void setDeviceToken(const QByteArray& value);

    // Clears the stored token. Called when the router rejects it (revoked remotely, password
    // changed elsewhere) so the next login walks the TOTP path again.
    void clearDeviceToken();

    // Direct access to ciphertext for database I/O.
    const QByteArray& encryptedDisplayName() const { return encrypted_display_name_; }
    void setEncryptedDisplayName(const QByteArray& blob) { encrypted_display_name_ = blob; }

    const QByteArray& encryptedAddress() const { return encrypted_address_; }
    void setEncryptedAddress(const QByteArray& blob) { encrypted_address_ = blob; }

    const QByteArray& encryptedUsername() const { return encrypted_username_; }
    void setEncryptedUsername(const QByteArray& blob) { encrypted_username_ = blob; }

    const QByteArray& encryptedPassword() const { return encrypted_password_; }
    void setEncryptedPassword(const QByteArray& blob) { encrypted_password_ = blob; }

    const QByteArray& encryptedDeviceToken() const { return encrypted_device_token_; }
    void setEncryptedDeviceToken(const QByteArray& blob)
        { encrypted_device_token_ = blob; }

private:
    qint64 router_id_ = -1;
    proto::router::SessionType session_type_;
    QByteArray encrypted_display_name_;
    QByteArray encrypted_address_;
    QByteArray encrypted_username_;
    QByteArray encrypted_password_;
    QByteArray encrypted_device_token_;
};

class HostConfig final
{
public:
    HostConfig() = default;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 groupId() const { return group_id_; }
    void setGroupId(qint64 id) { group_id_ = id; }

    qint64 routerId() const { return router_id_; }
    void setRouterId(qint64 id) { router_id_ = id; }

    qint64 createTime() const { return create_time_; }
    void setCreateTime(qint64 value) { create_time_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    qint64 connectTime() const { return connect_time_; }
    void setConnectTime(qint64 value) { connect_time_ = value; }

    QString name() const;
    void setName(const QString& value);

    QString comment() const;
    void setComment(const QString& value);

    QString address() const;
    void setAddress(const QString& value);

    QString username() const;
    void setUsername(const QString& value);

    SecureString password() const;
    void setPassword(const SecureString& value);

    // Direct access to ciphertext for database I/O.
    const QByteArray& encryptedName() const { return encrypted_name_; }
    void setEncryptedName(const QByteArray& blob) { encrypted_name_ = blob; }

    const QByteArray& encryptedComment() const { return encrypted_comment_; }
    void setEncryptedComment(const QByteArray& blob) { encrypted_comment_ = blob; }

    const QByteArray& encryptedAddress() const { return encrypted_address_; }
    void setEncryptedAddress(const QByteArray& blob) { encrypted_address_ = blob; }

    const QByteArray& encryptedUsername() const { return encrypted_username_; }
    void setEncryptedUsername(const QByteArray& blob) { encrypted_username_ = blob; }

    const QByteArray& encryptedPassword() const { return encrypted_password_; }
    void setEncryptedPassword(const QByteArray& blob) { encrypted_password_ = blob; }

private:
    qint64 id_ = -1;
    qint64 group_id_ = 0;
    qint64 router_id_ = 0;
    qint64 create_time_ = 0;
    qint64 modify_time_ = 0;
    qint64 connect_time_ = 0;
    QByteArray encrypted_name_;
    QByteArray encrypted_comment_;
    QByteArray encrypted_address_;
    QByteArray encrypted_username_;
    QByteArray encrypted_password_;
};

class GroupConfig final
{
public:
    GroupConfig() = default;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 parentId() const { return parent_id_; }
    void setParentId(qint64 id) { parent_id_ = id; }

    QString name() const;
    void setName(const QString& value);

    QString comment() const;
    void setComment(const QString& value);

    // Direct access to ciphertext for database I/O.
    const QByteArray& encryptedName() const { return encrypted_name_; }
    void setEncryptedName(const QByteArray& blob) { encrypted_name_ = blob; }

    const QByteArray& encryptedComment() const { return encrypted_comment_; }
    void setEncryptedComment(const QByteArray& blob) { encrypted_comment_ = blob; }

private:
    qint64 id_ = -1;
    qint64 parent_id_ = 0;
    QByteArray encrypted_name_;
    QByteArray encrypted_comment_;
};

proto::control::Config defaultDesktopConfig();

#endif // CLIENT_CONFIG_H
