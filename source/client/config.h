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
#include "base/net/proxy_settings.h"
#include "base/peer/host_id.h"
#include "proto/desktop_control.h"

namespace proto::router {
enum SessionType : int;
} // namespace proto::router

class RouterConfig final
{
public:
    RouterConfig();

    bool isValid() const;

    // The same router entered with the same account: address and user name.
    bool hasSameAccount(const RouterConfig& other) const;

    bool isSyncConfirmationNeeded(const RouterConfig& edited) const;

    proto::router::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::router::SessionType type) { session_type_ = type; }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enable) { enabled_ = enable; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    const QString& address() const { return address_; }
    void setAddress(const QString& value) { address_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    const QByteArray& deviceToken() const { return device_token_; }
    void setDeviceToken(const QByteArray& value) { device_token_ = value; }

    void clearDeviceToken() { device_token_.clear(); }

    const QString& userGuid() const { return user_guid_; }
    void setUserGuid(const QString& value) { user_guid_ = value; }

    const QByteArray& syncStorageId() const { return sync_storage_id_; }
    void setSyncStorageId(const QByteArray& value) { sync_storage_id_ = value; }

    const SecureByteArray& syncStorageKey() const { return sync_storage_key_; }
    void setSyncStorageKey(const SecureByteArray& value) { sync_storage_key_ = value; }

    bool isSyncEnabled() const { return sync_enabled_; }
    void setSyncEnabled(bool enable) { sync_enabled_ = enable; }

    bool isSyncStorageResetPending() const { return sync_storage_reset_pending_; }
    void setSyncStorageResetPending(bool pending) { sync_storage_reset_pending_ = pending; }

    // The encrypted column for database I/O. Returns nothing when the record cannot be sealed, so a
    // failure is never written as a record that simply holds nothing. The setter answers whether the
    // column opened; on a refusal the fields it carries are left empty.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    proto::router::SessionType session_type_;
    bool enabled_ = true;
    QString guid_;
    QString address_;
    QString username_;
    SecureString password_;
    QByteArray device_token_;
    QString user_guid_;
    QByteArray sync_storage_id_;
    SecureByteArray sync_storage_key_;
    bool sync_enabled_ = false;
    bool sync_storage_reset_pending_ = false;
};

class RouterHostConfig final
{
public:
    RouterHostConfig() = default;

    bool isValid() const;

    HostId hostId() const { return host_id_; }
    void setHostId(HostId id) { host_id_ = id; }

    qint64 credentialId() const { return credential_id_; }
    void setCredentialId(qint64 id) { credential_id_ = id; }

    qint64 osCredentialId() const { return os_credential_id_; }
    void setOsCredentialId(qint64 id) { os_credential_id_ = id; }

    qint64 profileId() const { return profile_id_; }
    void setProfileId(qint64 id) { profile_id_ = id; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    const QByteArray& unknownData() const { return unknown_data_; }
    void setUnknownData(const QByteArray& value) { unknown_data_ = value; }

    // The sealed column for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    HostId host_id_ = kInvalidHostId;
    qint64 credential_id_ = 0;
    qint64 os_credential_id_ = 0;
    qint64 profile_id_ = 0;
    QString username_;
    SecureString password_;
    qint64 modify_time_ = 0;
    QByteArray unknown_data_;
};

class LocalHostConfig final
{
public:
    LocalHostConfig() = default;

    static constexpr int kMaxNameLength = 64;
    static constexpr int kMaxCommentLength = 2048;

    // The credentials are kept as a whole pair, because an empty pair means the host is asked for
    // them at every connection.
    bool isValid() const;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 groupId() const { return group_id_; }
    void setGroupId(qint64 id) { group_id_ = id; }

    qint64 credentialId() const { return credential_id_; }
    void setCredentialId(qint64 id) { credential_id_ = id; }

    qint64 osCredentialId() const { return os_credential_id_; }
    void setOsCredentialId(qint64 id) { os_credential_id_ = id; }

    qint64 profileId() const { return profile_id_; }
    void setProfileId(qint64 id) { profile_id_ = id; }

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

    const QByteArray& unknownData() const { return unknown_data_; }
    void setUnknownData(const QByteArray& value) { unknown_data_ = value; }

    // The sealed column for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    qint64 id_ = -1;
    qint64 group_id_ = 0;
    qint64 credential_id_ = 0;
    qint64 os_credential_id_ = 0;
    qint64 profile_id_ = 0;
    QString guid_;
    qint64 create_time_ = 0;
    qint64 modify_time_ = 0;
    qint64 connect_time_ = 0;
    QString name_;
    QString comment_;
    QString address_;
    QString username_;
    SecureString password_;
    QByteArray unknown_data_;
};

class LocalGroupConfig final
{
public:
    LocalGroupConfig() = default;

    static constexpr int kMaxNameLength = 64;
    static constexpr int kMaxCommentLength = 2048;

    bool isValid() const;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    qint64 parentId() const { return parent_id_; }
    void setParentId(qint64 id) { parent_id_ = id; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const QString& comment() const { return comment_; }
    void setComment(const QString& value) { comment_ = value; }

    const QByteArray& unknownData() const { return unknown_data_; }
    void setUnknownData(const QByteArray& value) { unknown_data_ = value; }

    // The data column for database I/O.
    QByteArray serializedData() const;
    bool setSerializedData(const QByteArray& data);

private:
    qint64 id_ = -1;
    qint64 parent_id_ = 0;
    QString guid_;
    qint64 modify_time_ = 0;
    QString name_;
    QString comment_;
    QByteArray unknown_data_;
};

class CredentialConfig final
{
public:
    CredentialConfig() = default;

    enum class Type { ASPIA = 0, OS = 1 };

    static constexpr int kMaxNameLength = 64;

    // The pair is kept whole, and a record is shown by its name.
    bool isValid() const;

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    Type type() const { return type_; }
    void setType(Type value) { type_ = value; }

    const QString& displayName() const { return display_name_; }
    void setDisplayName(const QString& value) { display_name_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    const QByteArray& unknownData() const { return unknown_data_; }
    void setUnknownData(const QByteArray& value) { unknown_data_ = value; }

    // The sealed column for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    qint64 id_ = -1;
    QString guid_;
    qint64 modify_time_ = 0;
    Type type_ = Type::ASPIA;
    QString display_name_;
    QString username_;
    SecureString password_;
    QByteArray unknown_data_;
};

class ProfileConfig final
{
public:
    ProfileConfig() = default;

    static constexpr int kMaxNameLength = 64;

    static constexpr char kDefaultGuid[] = "00000000-0000-0000-0000-000000000000";

    bool isValid() const;
    bool isDefault() const { return guid_ == kDefaultGuid; }

    qint64 id() const { return id_; }
    void setId(qint64 id) { id_ = id; }

    const QString& guid() const { return guid_; }
    void setGuid(const QString& value) { guid_ = value; }

    qint64 modifyTime() const { return modify_time_; }
    void setModifyTime(qint64 value) { modify_time_ = value; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const proto::control::Config& desktopConfig() const { return desktop_config_; }
    void setDesktopConfig(const proto::control::Config& config) { desktop_config_ = config; }

    quint32 udpMethods() const { return udp_methods_; }
    void setUdpMethods(quint32 methods) { udp_methods_ = methods; }

    const QByteArray& unknownData() const { return unknown_data_; }
    void setUnknownData(const QByteArray& value) { unknown_data_ = value; }

    // The data column for database I/O.
    QByteArray serializedData() const;
    bool setSerializedData(const QByteArray& data);

private:
    qint64 id_ = -1;
    QString guid_;
    qint64 modify_time_ = 0;
    QString name_;
    proto::control::Config desktop_config_;
    quint32 udp_methods_ = 0;
    QByteArray unknown_data_;
};

class ProxyConfig final
{
public:
    ProxyConfig() = default;

    const ProxySettings& settings() const { return settings_; }
    void setSettings(const ProxySettings& settings) { settings_ = settings; }

    // The sealed value for database I/O. See RouterConfig for what the two answer.
    std::optional<QByteArray> encryptedData() const;
    bool setEncryptedData(const QByteArray& blob);

private:
    ProxySettings settings_;
};

class HostConfig final
{
public:
    HostConfig() = default;

    static HostConfig forLocalHost(const LocalHostConfig& host);
    static HostConfig forRouterHost(HostId host_id, const QString& name);

    // The local host the connection was built from, 0 for a host of a router.
    qint64 entryId() const { return entry_id_; }
    void setEntryId(qint64 id) { entry_id_ = id; }

    const QString& address() const { return address_; }
    void setAddress(const QString& value) { address_ = value; }

    const QString& name() const { return name_; }
    void setName(const QString& value) { name_ = value; }

    const QString& username() const { return username_; }
    void setUsername(const QString& value) { username_ = value; }

    const SecureString& password() const { return password_; }
    void setPassword(const SecureString& value) { password_ = value; }

    bool isPortable() const { return portable_; }
    void setPortable(bool portable) { portable_ = portable; }

private:
    qint64 entry_id_ = 0;
    QString address_;
    QString name_;
    QString username_;
    SecureString password_;
    bool portable_ = false;
};

const quint32 kAutoQuality = 0;
const quint32 kLowQuality = 1;
const quint32 kMediumQuality = 5;
const quint32 kHighQuality = 10;

proto::control::Config defaultDesktopConfig();

// The directory chosen in |config|, or the default one when none is chosen.
QString recordingPath(const proto::control::Config& config);

#endif // CLIENT_CONFIG_H
