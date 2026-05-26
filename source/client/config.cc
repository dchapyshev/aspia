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
#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "proto/desktop_control.h"
#include "proto/router.h"

namespace {

QByteArray encryptBytes(const QByteArray& plain)
{
    if (plain.isEmpty())
        return QByteArray();
    DataCryptor& cryptor = DataCryptor::instance();
    CHECK(cryptor.isValid());
    return cryptor.encrypt(plain).value_or(QByteArray());
}

QByteArray decryptBytes(const QByteArray& blob)
{
    if (blob.isEmpty())
        return QByteArray();
    DataCryptor& cryptor = DataCryptor::instance();
    CHECK(cryptor.isValid());
    return cryptor.decrypt(blob).value_or(QByteArray());
}

QByteArray encryptString(const QString& value)
{
    return encryptBytes(value.toUtf8());
}

QString decryptString(const QByteArray& blob)
{
    return QString::fromUtf8(decryptBytes(blob));
}

QByteArray encryptSecureString(const SecureString& value)
{
    return encryptBytes(SecureByteArray(value.toUtf8()).toByteArray());
}

SecureString decryptSecureString(const QByteArray& blob)
{
    if (blob.isEmpty())
        return SecureString();
    return SecureString::fromUtf8(SecureByteArray(decryptBytes(blob)));
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
    return !encrypted_address_.isEmpty() && !encrypted_username_.isEmpty() &&
           !encrypted_password_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool RouterConfig::hasSameParams(const RouterConfig& other) const
{
    return address() == other.address() && session_type_ == other.session_type_ &&
           username() == other.username() && password() == other.password();
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::displayLabel() const
{
    QString name = displayName();
    if (!name.isEmpty())
        return name;
    return address();
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::displayName() const
{
    return decryptString(encrypted_display_name_);
}

//--------------------------------------------------------------------------------------------------
void RouterConfig::setDisplayName(const QString& value)
{
    encrypted_display_name_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::address() const
{
    return decryptString(encrypted_address_);
}

//--------------------------------------------------------------------------------------------------
void RouterConfig::setAddress(const QString& value)
{
    encrypted_address_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString RouterConfig::username() const
{
    return decryptString(encrypted_username_);
}

//--------------------------------------------------------------------------------------------------
void RouterConfig::setUsername(const QString& value)
{
    encrypted_username_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
SecureString RouterConfig::password() const
{
    return decryptSecureString(encrypted_password_);
}

//--------------------------------------------------------------------------------------------------
void RouterConfig::setPassword(const SecureString& value)
{
    encrypted_password_ = encryptSecureString(value);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterConfig::data() const
{
    return decryptBytes(encrypted_data_);
}

//--------------------------------------------------------------------------------------------------
void RouterConfig::setData(const QByteArray& value)
{
    encrypted_data_ = encryptBytes(value);
}

//--------------------------------------------------------------------------------------------------
QString HostConfig::name() const
{
    return decryptString(encrypted_name_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setName(const QString& value)
{
    encrypted_name_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString HostConfig::comment() const
{
    return decryptString(encrypted_comment_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setComment(const QString& value)
{
    encrypted_comment_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString HostConfig::address() const
{
    return decryptString(encrypted_address_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setAddress(const QString& value)
{
    encrypted_address_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString HostConfig::username() const
{
    return decryptString(encrypted_username_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setUsername(const QString& value)
{
    encrypted_username_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
SecureString HostConfig::password() const
{
    return decryptSecureString(encrypted_password_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setPassword(const SecureString& value)
{
    encrypted_password_ = encryptSecureString(value);
}

//--------------------------------------------------------------------------------------------------
QByteArray HostConfig::data() const
{
    return decryptBytes(encrypted_data_);
}

//--------------------------------------------------------------------------------------------------
void HostConfig::setData(const QByteArray& value)
{
    encrypted_data_ = encryptBytes(value);
}

//--------------------------------------------------------------------------------------------------
QString GroupConfig::name() const
{
    return decryptString(encrypted_name_);
}

//--------------------------------------------------------------------------------------------------
void GroupConfig::setName(const QString& value)
{
    encrypted_name_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QString GroupConfig::comment() const
{
    return decryptString(encrypted_comment_);
}

//--------------------------------------------------------------------------------------------------
void GroupConfig::setComment(const QString& value)
{
    encrypted_comment_ = encryptString(value);
}

//--------------------------------------------------------------------------------------------------
QByteArray GroupConfig::data() const
{
    return decryptBytes(encrypted_data_);
}

//--------------------------------------------------------------------------------------------------
void GroupConfig::setData(const QByteArray& value)
{
    encrypted_data_ = encryptBytes(value);
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
