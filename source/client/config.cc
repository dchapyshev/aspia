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

#include "client/config.h"

#include <QStandardPaths>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "base/net/address.h"
#include "proto/desktop_control.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

const char kLocalHostsAad[] = "local_hosts";
const char kRoutersAad[] = "routers";
const char kProxyAad[] = "settings/proxy";

QByteArray routerHostAad(HostId host_id)
{
    return QByteArrayLiteral("router_hosts/") + QByteArray::number(host_id);
}

//--------------------------------------------------------------------------------------------------
QByteArray credentialAad(const QString& guid)
{
    return QByteArrayLiteral("credentials/") + guid.toUtf8();
}

SecureString toSecureString(const std::string& value)
{
    return SecureString::fromUtf8(
        SecureByteArray(value.data(), static_cast<qsizetype>(value.size())));
}

template <class Message>
std::optional<QByteArray> sealMessage(const Message& message, QByteArrayView aad)
{
    DataCryptor& cryptor = DataCryptor::instance();
    CHECK(cryptor.isValid());

    const SecureByteArray buffer(serialize(message));

    std::optional<QByteArray> sealed = cryptor.encrypt(buffer.toByteArray(), aad);
    if (!sealed.has_value())
        LOG(ERROR) << "Unable to encrypt record data";

    return sealed;
}

template <class Message>
bool unsealMessage(const QByteArray& blob, QByteArrayView aad, Message* message)
{
    DataCryptor& cryptor = DataCryptor::instance();
    CHECK(cryptor.isValid());

    std::optional<QByteArray> decrypted = cryptor.decrypt(blob, aad);
    if (!decrypted.has_value())
    {
        LOG(ERROR) << "Unable to decrypt record data";
        return false;
    }

    const SecureByteArray plain(std::move(*decrypted));

    if (!parse(plain.toByteArray(), message))
    {
        LOG(ERROR) << "Unable to parse record data";
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterConfig::RouterConfig()
    : session_type_(proto::router::SESSION_TYPE_OPERATOR)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::isValid() const
{
    return !address_.isEmpty() && !username_.isEmpty() && !password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::hasSameAccount(const RouterConfig& other) const
{
    if (username_.compare(other.username_, Qt::CaseInsensitive) != 0)
        return false;

    const Address address = Address::fromString(address_, kDefaultRouterClientTcpPort);
    const Address other_address = Address::fromString(other.address_, kDefaultRouterClientTcpPort);

    if (!address.isValid() || !other_address.isValid())
        return address_ == other.address_;

    return address.port() == other_address.port() &&
           address.host().compare(other_address.host(), Qt::CaseInsensitive) == 0;
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::isSyncConfirmationNeeded(const RouterConfig& edited) const
{
    return edited.sync_enabled_ && !(sync_enabled_ && (hasSameAccount(edited) || !user_guid_.isEmpty()));
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> RouterConfig::encryptedData() const
{
    proto::storage::RouterBlob data;
    data.set_address(address_.toStdString());
    data.set_username(username_.toStdString());
    data.set_device_token(device_token_.toStdString());
    data.set_user_guid(user_guid_.toStdString());
    data.set_session_type(static_cast<quint32>(session_type_));
    data.set_enabled(enabled_);

    proto::storage::RouterBlob::Sync* sync = data.mutable_sync();
    sync->set_enabled(sync_enabled_);
    sync->set_storage_id(sync_storage_id_.toStdString());
    sync->set_storage_key(sync_storage_key_.constData(), static_cast<size_t>(sync_storage_key_.size()));
    sync->set_storage_reset_pending(sync_storage_reset_pending_);

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, kRoutersAad);

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());
    memZero(sync->mutable_storage_key());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::setEncryptedData(const QByteArray& blob)
{
    address_.clear();
    username_.clear();
    password_.clear();
    device_token_.clear();
    user_guid_.clear();
    sync_storage_id_.clear();
    sync_storage_key_.clear();
    sync_enabled_ = false;
    sync_storage_reset_pending_ = false;
    session_type_ = proto::router::SESSION_TYPE_OPERATOR;
    enabled_ = true;

    if (blob.isEmpty())
        return true;

    proto::storage::RouterBlob data;
    if (!unsealMessage(blob, kRoutersAad, &data))
        return false;

    address_ = QString::fromStdString(data.address());
    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());
    device_token_ = QByteArray::fromStdString(data.device_token());
    user_guid_ = QString::fromStdString(data.user_guid());
    session_type_ = static_cast<proto::router::SessionType>(data.session_type());
    enabled_ = data.enabled();

    proto::storage::RouterBlob::Sync* sync = data.mutable_sync();
    sync_enabled_ = sync->enabled();
    sync_storage_id_ = QByteArray::fromStdString(sync->storage_id());
    sync_storage_key_ = SecureByteArray(sync->storage_key().data(), static_cast<qsizetype>(sync->storage_key().size()));
    sync_storage_reset_pending_ = sync->storage_reset_pending();

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());
    memZero(sync->mutable_storage_key());

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostConfig::isValid() const
{
    return host_id_ != kInvalidHostId && !isTempHostId(host_id_) && username_.isEmpty() == password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> RouterHostConfig::encryptedData() const
{
    if (username_.isEmpty() && password_.isEmpty())
        return QByteArray();

    proto::storage::HostBlob data;
    data.set_username(username_.toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, routerHostAad(host_id_));

    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostConfig::setEncryptedData(const QByteArray& blob)
{
    username_.clear();
    password_.clear();

    if (blob.isEmpty())
        return true;

    proto::storage::HostBlob data;
    if (!unsealMessage(blob, routerHostAad(host_id_), &data))
        return false;

    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());

    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostConfig::isValid() const
{
    if (name_.isEmpty() || name_.length() > kMaxNameLength ||
        comment_.length() > kMaxCommentLength)
    {
        return false;
    }

    return !address_.isEmpty() && group_id_ >= 0 && username_.isEmpty() == password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> LocalHostConfig::encryptedData() const
{
    proto::storage::HostBlob data;
    data.set_address(address_.toStdString());
    data.set_username(username_.toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, kLocalHostsAad);

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostConfig::setEncryptedData(const QByteArray& blob)
{
    address_.clear();
    username_.clear();
    password_.clear();

    if (blob.isEmpty())
        return true;

    proto::storage::HostBlob data;
    if (!unsealMessage(blob, kLocalHostsAad, &data))
        return false;

    address_ = QString::fromStdString(data.address());
    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupConfig::isValid() const
{
    return !name_.isEmpty() && name_.length() <= kMaxNameLength &&
           comment_.length() <= kMaxCommentLength;
}

//--------------------------------------------------------------------------------------------------
bool CredentialConfig::isValid() const
{
    return !display_name_.isEmpty() && display_name_.length() <= kMaxNameLength &&
           !username_.isEmpty() && !password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> CredentialConfig::encryptedData() const
{
    proto::storage::CredentialBlob data;
    data.set_username(username_.toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));
    data.set_unknown_data(unknown_data_.constData(), static_cast<size_t>(unknown_data_.size()));

    std::optional<QByteArray> sealed = sealMessage(data, credentialAad(guid_));

    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool CredentialConfig::setEncryptedData(const QByteArray& blob)
{
    username_.clear();
    password_.clear();
    unknown_data_.clear();

    if (blob.isEmpty())
        return true;

    proto::storage::CredentialBlob data;
    if (!unsealMessage(blob, credentialAad(guid_), &data))
        return false;

    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());
    unknown_data_ = QByteArray::fromStdString(data.unknown_data());

    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ProfileConfig::isValid() const
{
    if (isDefault())
        return name_.isEmpty();

    return !name_.isEmpty() && name_.length() <= kMaxNameLength;
}

//--------------------------------------------------------------------------------------------------
QByteArray ProfileConfig::serializedData() const
{
    proto::storage::ProfileData data;
    data.mutable_desktop()->CopyFrom(desktop_config_);
    data.set_udp_methods(udp_methods_);
    return serialize(data);
}

//--------------------------------------------------------------------------------------------------
bool ProfileConfig::setSerializedData(const QByteArray& data)
{
    proto::storage::ProfileData profile;
    if (!parse(data, &profile))
        return false;

    desktop_config_ = profile.desktop();
    udp_methods_ = profile.udp_methods();
    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> ProxyConfig::encryptedData() const
{
    if (settings_ == ProxySettings())
        return QByteArray();

    proto::storage::ProxyBlob data;
    data.set_enabled(settings_.enabled);
    data.set_type(static_cast<quint32>(settings_.type));
    data.set_address(settings_.address.toStdString());
    data.set_port(settings_.port);
    data.set_username(settings_.user.toStdString());

    const SecureByteArray password = settings_.password.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, kProxyAad);

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool ProxyConfig::setEncryptedData(const QByteArray& blob)
{
    settings_ = ProxySettings();

    if (blob.isEmpty())
        return true;

    proto::storage::ProxyBlob data;
    if (!unsealMessage(blob, kProxyAad, &data))
        return false;

    const bool valid = data.type() <= static_cast<quint32>(ProxySettings::Type::SOCKS5H) &&
                       data.port() <= 0xFFFF;
    if (valid)
    {
        settings_.enabled = data.enabled();
        settings_.type = static_cast<ProxySettings::Type>(data.type());
        settings_.address = QString::fromStdString(data.address());
        settings_.port = static_cast<quint16>(data.port());
        settings_.user = QString::fromStdString(data.username());
        settings_.password = toSecureString(data.password());
    }
    else
    {
        LOG(ERROR) << "Invalid proxy data";
    }

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return valid;
}

//--------------------------------------------------------------------------------------------------
// static
HostConfig HostConfig::forLocalHost(const LocalHostConfig& host)
{
    HostConfig config;
    config.setEntryId(host.id());
    config.setAddress(host.address());
    config.setName(host.name());
    config.setUsername(host.username());
    config.setPassword(host.password());
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
HostConfig HostConfig::forRouterHost(HostId host_id, const QString& name)
{
    HostConfig config;
    config.setAddress(hostIdToString(host_id));
    config.setName(name);
    return config;
}

//--------------------------------------------------------------------------------------------------
proto::control::Config defaultDesktopConfig()
{
    proto::control::Config config;
    config.set_audio(true);
    config.set_cursor_shape(true);
    config.set_cursor_position(false);
    config.set_clipboard(true);
    config.set_effects(false);
    config.set_wallpaper(false);
    config.set_block_input(false);
    config.set_lock_at_disconnect(false);
    config.set_disable_preferred_size(false);
    config.set_quality(kAutoQuality);
    config.set_disable_key_combinations(false);
    config.set_record_sessions(false);
    return config;
}

//--------------------------------------------------------------------------------------------------
QString recordingPath(const proto::control::Config& config)
{
    QString path = QString::fromStdString(config.recording_path());
    if (path.isEmpty())
        path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/Aspia";
    return path;
}
