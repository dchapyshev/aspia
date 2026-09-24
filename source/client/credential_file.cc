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

#include "client/credential_file.h"

#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <optional>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "client/config.h"
#include "proto/storage.h"

namespace {

constexpr int kFormatVersion = 1;
constexpr int kSaltSize = 32;
constexpr int kVerifierPayloadSize = 32;

using FileContent = proto::storage::CredentialFile::Content;
using FileCredential = proto::storage::CredentialFile::Credential;

//--------------------------------------------------------------------------------------------------
void eraseSecretFields(FileContent* content)
{
    for (FileCredential& credential : *content->mutable_credentials())
    {
        memZero(credential.mutable_username());
        memZero(credential.mutable_password());
    }
}

//--------------------------------------------------------------------------------------------------
bool isValidGuid(const QString& guid)
{
    return !guid.isEmpty() && QUuid::fromString(guid).toString(QUuid::WithoutBraces) == guid;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray fileKey(const SecureString& password, const QByteArray& salt)
{
    return SecureByteArray(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
CredentialFile::Result CredentialFile::exportToFile(
    const QList<CredentialConfig>& credentials, const QString& file_path, const SecureString& password)
{
    FileContent content;

    for (const CredentialConfig& credential : credentials)
    {
        if (!credential.isValid())
        {
            eraseSecretFields(&content);
            return Result::UNREADABLE_RECORD;
        }

        FileCredential* out = content.add_credentials();
        out->set_guid(credential.guid().toStdString());
        out->set_type(static_cast<quint32>(credential.type()));
        out->set_display_name(credential.displayName().toStdString());
        out->set_username(credential.username().toStdString());

        const SecureByteArray secret = credential.password().toUtf8();
        out->set_password(secret.constData(), static_cast<size_t>(secret.size()));
    }

    const QByteArray salt = Random::byteArray(kSaltSize);
    DataCryptor cryptor(CipherType::AES256_GCM, fileKey(password, salt));

    const std::optional<QByteArray> verifier = cryptor.encrypt(Random::byteArray(kVerifierPayloadSize));

    std::optional<QByteArray> sealed;
    {
        const SecureByteArray buffer(serialize(content));
        sealed = cryptor.encrypt(buffer.toByteArray());
    }

    eraseSecretFields(&content);

    if (!verifier.has_value() || !sealed.has_value())
    {
        LOG(ERROR) << "Unable to encrypt credentials";
        return Result::INTERNAL_ERROR;
    }

    proto::storage::CredentialFile file_message;
    file_message.set_version(kFormatVersion);
    file_message.set_salt(salt.toStdString());
    file_message.set_verifier(verifier->toStdString());
    file_message.set_data(sealed->toStdString());

    const QByteArray payload = serialize(file_message);
    if (payload.isEmpty())
    {
        LOG(ERROR) << "Unable to serialize credentials file";
        return Result::INTERNAL_ERROR;
    }

    // A document picked on Android is a content URI, which has no directory for a temporary
    // file, so it is written in place.
    QSaveFile file(file_path);
    file.setDirectWriteFallback(true);

    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    if (file.write(payload) != payload.size() || !file.commit())
    {
        LOG(ERROR) << "Unable to write file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
CredentialFile::Result CredentialFile::importFromFile(
    const QString& file_path, const SecureString& password, QList<CredentialConfig>* credentials)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file" << file_path << ":" << file.errorString();
        return Result::FILE_ERROR;
    }

    const QByteArray buffer = file.readAll();
    file.close();

    proto::storage::CredentialFile file_message;
    if (buffer.isEmpty() || !parse(buffer, &file_message))
        return Result::INVALID_FORMAT;

    const QByteArray salt = QByteArray::fromStdString(file_message.salt());
    if (salt.size() != kSaltSize || file_message.verifier().empty() || file_message.data().empty())
        return Result::INVALID_FORMAT;

    if (file_message.version() != kFormatVersion)
        return Result::UNSUPPORTED_VERSION;

    DataCryptor cryptor(CipherType::AES256_GCM, fileKey(password, salt));

    if (!cryptor.decrypt(QByteArray::fromStdString(file_message.verifier())).has_value())
        return Result::WRONG_PASSWORD;

    std::optional<QByteArray> decrypted = cryptor.decrypt(QByteArray::fromStdString(file_message.data()));
    if (!decrypted.has_value())
    {
        // The password is already proven right, so this is damage.
        LOG(ERROR) << "Unable to decrypt credentials";
        return Result::INVALID_FORMAT;
    }

    const SecureByteArray plain(std::move(*decrypted));

    FileContent content;
    if (!parse(plain.toByteArray(), &content))
    {
        LOG(ERROR) << "Unable to parse credentials";
        eraseSecretFields(&content);
        return Result::INVALID_FORMAT;
    }

    QList<CredentialConfig> result;
    QSet<QString> guids;

    for (const FileCredential& credential : content.credentials())
    {
        const QString guid = QString::fromStdString(credential.guid());

        CredentialConfig config;
        config.setGuid(guid);
        config.setDisplayName(QString::fromStdString(credential.display_name()));
        config.setUsername(QString::fromStdString(credential.username()));
        config.setPassword(SecureString(QString::fromStdString(credential.password())));

        if (!config.isValid() || credential.type() != static_cast<quint32>(CredentialConfig::Type::HOST) ||
            !isValidGuid(guid) || guids.contains(guid))
        {
            LOG(ERROR) << "The file carries an invalid record";
            eraseSecretFields(&content);
            return Result::INVALID_FORMAT;
        }

        guids.insert(guid);
        result.append(config);
    }

    eraseSecretFields(&content);

    *credentials = std::move(result);
    return Result::SUCCESS;
}
