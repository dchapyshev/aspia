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
#include "base/crypto/os_crypt.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "proto/client_storage.h"
#include "proto/desktop_control.h"
#include "proto/router.h"

namespace {

// Names the table a column was sealed for. A host and a router number their fields the same way, so
// without this a router column moved into a host row would parse as a host one and open.
const char kHostsAad[] = "hosts";
const char kRoutersAad[] = "routers";

SecureString toSecureString(const std::string& value)
{
    return SecureString::fromUtf8(
        SecureByteArray(value.data(), static_cast<qsizetype>(value.size())));
}

template <class Message>
std::optional<QByteArray> sealMessage(const Message& message, const char* aad)
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
bool unsealMessage(const QByteArray& blob, const char* aad, Message* message)
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
    : session_type_(proto::router::SESSION_TYPE_CLIENT)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::isValid() const
{
    return !address_.isEmpty() && !username_.isEmpty() && !password_.isEmpty();
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
    // The token goes under the keystore of the user first. A token that cannot be wrapped must not
    // be stored bare, and the record is refused rather than written without it.
    QByteArray wrapped_token;
    if (!device_token_.isEmpty())
    {
        if (!OSCrypt::encryptBytes(device_token_, &wrapped_token) || wrapped_token.isEmpty())
        {
            LOG(ERROR) << "OSCrypt::encryptBytes failed for device token";
            return std::nullopt;
        }
    }

    proto::client_storage::RouterData data;
    data.set_address(address_.toUtf8().toStdString());
    data.set_username(username_.toUtf8().toStdString());
    data.set_device_token(wrapped_token.toStdString());

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

    proto::client_storage::RouterData data;
    if (!unsealMessage(blob, kRoutersAad, &data))
        return false;

    address_ = QString::fromStdString(data.address());
    username_ = QString::fromStdString(data.username());
    password_ = toSecureString(data.password());

    // A token wrapped for another user of another machine is not an error of the record: the router
    // will ask for a TOTP code again and a new one will be issued.
    if (!data.device_token().empty())
    {
        QByteArray token;
        if (OSCrypt::decryptBytes(QByteArray::fromStdString(data.device_token()), &token))
            device_token_ = token;
        else
            LOG(ERROR) << "OSCrypt::decryptBytes failed for device token";
    }

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> HostConfig::encryptedData() const
{
    proto::client_storage::HostData data;
    data.set_address(address_.toUtf8().toStdString());
    data.set_username(username_.toUtf8().toStdString());

    const SecureByteArray password = password_.toUtf8();
    data.set_password(password.constData(), static_cast<size_t>(password.size()));

    std::optional<QByteArray> sealed = sealMessage(data, kHostsAad);

    memZero(data.mutable_address());
    memZero(data.mutable_username());
    memZero(data.mutable_password());

    return sealed;
}

//--------------------------------------------------------------------------------------------------
bool HostConfig::setEncryptedData(const QByteArray& blob)
{
    address_.clear();
    username_.clear();
    password_.clear();

    if (blob.isEmpty())
        return true;

    proto::client_storage::HostData data;
    if (!unsealMessage(blob, kHostsAad, &data))
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
