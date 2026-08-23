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

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "proto/desktop_control.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

// Names the table a column was sealed for. A host and a router number their fields the same way, so
// without this a router column moved into a host row would parse as a host one and open.
const char kLocalHostsAad[] = "local_hosts";
const char kRoutersAad[] = "routers";

// The credentials of a router host are sealed for their row, both halves of its key. Every column
// of the table opens with the same key, so a blob moved into the row of another host, or of the
// same host under another router, would otherwise open and hand out credentials the user never
// saved for it.
QByteArray routerHostAad(qint64 router_id, HostId host_id)
{
    return QByteArrayLiteral("router_hosts/") + QByteArray::number(router_id) + '/' +
           QByteArray::number(host_id);
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
    return !address_.isEmpty() && !username_.isEmpty() && !password_.isEmpty() &&
           display_name_.length() <= kMaxNameLength;
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::hasSameParams(const RouterConfig& other) const
{
    return address_ == other.address_ && session_type_ == other.session_type_ &&
           username_ == other.username_ && password_ == other.password_;
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::displayLabel() const
{
    if (!display_name_.isEmpty())
        return display_name_;
    return address_;
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> RouterConfig::encryptedData() const
{
    proto::storage::RouterBlob data;
    data.set_address(address_.toStdString());
    data.set_username(username_.toStdString());
    data.set_device_token(device_token_.toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, kRoutersAad);

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::setEncryptedData(const QByteArray& blob)
{
    address_.clear();
    username_.clear();
    password_.clear();
    device_token_.clear();

    if (blob.isEmpty())
        return true;

    proto::storage::RouterBlob data;
    if (!unsealMessage(blob, kRoutersAad, &data))
        return false;

    address_ = QString::fromStdString(data.address());
    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());
    device_token_ = QByteArray::fromStdString(data.device_token());

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostConfig::isValid() const
{
    return router_id_ > 0 && host_id_ != kInvalidHostId && !isTempHostId(host_id_) &&
           !username_.isEmpty() && !password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> RouterHostConfig::encryptedData() const
{
    proto::storage::HostBlob data;
    data.set_username(username_.toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, routerHostAad(router_id_, host_id_));

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
    if (!unsealMessage(blob, routerHostAad(router_id_, host_id_), &data))
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
// static
HostConfig HostConfig::forLocalHost(const LocalHostConfig& host)
{
    HostConfig config;
    config.setEntryId(host.id());
    config.setRouterId(host.routerId());
    config.setAddress(host.address());
    config.setName(host.name());
    config.setUsername(host.username());
    config.setPassword(host.password());
    return config;
}

//--------------------------------------------------------------------------------------------------
// static
HostConfig HostConfig::forRouterHost(qint64 router_id, HostId host_id, const QString& name)
{
    HostConfig config;
    config.setRouterId(router_id);
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
    return config;
}
