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
#include <QTemporaryDir>
#include <QUuid>

#include <gtest/gtest.h>

#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "client/backup.h"
#include "client/config.h"
#include "client/database.h"
#include "proto/storage.h"

namespace {

//--------------------------------------------------------------------------------------------------
CredentialConfig makeCredential(const QString& name, const QString& username, const QString& password)
{
    CredentialConfig credential;
    credential.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
    credential.setDisplayName(name);
    credential.setUsername(username);
    credential.setPassword(SecureString(password));
    return credential;
}

//--------------------------------------------------------------------------------------------------
bool writeFile(const QString& file_path, const QByteArray& data)
{
    QFile file(file_path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

//--------------------------------------------------------------------------------------------------
class CredentialFileTest : public testing::Test
{
protected:
    static SecureString password() { return SecureString(QString("Password123")); }

    QString filePath() const { return dir_.filePath("credentials.aspia-credentials"); }

    // Seals |content| the way an export does, so that a test can hand the import a file an export
    // would never write.
    bool writeContent(const proto::storage::CredentialFile::Content& content)
    {
        const QByteArray salt = Random::byteArray(32);
        DataCryptor cryptor(CipherType::AES256_GCM,
            SecureByteArray(PasswordHash::hash(PasswordHash::ARGON2ID, password(), salt)));

        const std::optional<QByteArray> verifier = cryptor.encrypt(Random::byteArray(32));
        const std::optional<QByteArray> data = cryptor.encrypt(serialize(content));
        if (!verifier.has_value() || !data.has_value())
            return false;

        proto::storage::CredentialFile file_message;
        file_message.set_version(1);
        file_message.set_salt(salt.toStdString());
        file_message.set_verifier(verifier->toStdString());
        file_message.set_data(data->toStdString());

        return writeFile(filePath(), serialize(file_message));
    }

    QTemporaryDir dir_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialFileTest, RecordsTravel)
{
    ASSERT_TRUE(dir_.isValid());

    const QList<CredentialConfig> exported =
    {
        makeCredential("first", "user1", "secret1"),
        makeCredential("second", "user2", "secret2")
    };

    ASSERT_EQ(CredentialFile::exportToFile(exported, filePath(), password()),
              CredentialFile::Result::SUCCESS);

    QList<CredentialConfig> imported;
    ASSERT_EQ(CredentialFile::importFromFile(filePath(), password(), &imported),
              CredentialFile::Result::SUCCESS);

    ASSERT_EQ(imported.size(), exported.size());

    for (int i = 0; i < imported.size(); ++i)
    {
        EXPECT_EQ(imported[i].id(), -1);
        EXPECT_EQ(imported[i].guid(), exported[i].guid());
        EXPECT_EQ(imported[i].type(), exported[i].type());
        EXPECT_EQ(imported[i].displayName(), exported[i].displayName());
        EXPECT_EQ(imported[i].username(), exported[i].username());
        EXPECT_EQ(imported[i].password(), exported[i].password());
    }
}

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialFileTest, WrongPasswordIsTold)
{
    ASSERT_TRUE(dir_.isValid());
    ASSERT_EQ(CredentialFile::exportToFile({ makeCredential("name", "user", "secret") }, filePath(),
                                           password()),
              CredentialFile::Result::SUCCESS);

    QList<CredentialConfig> imported = { makeCredential("kept", "user", "secret") };
    EXPECT_EQ(CredentialFile::importFromFile(filePath(), SecureString(QString("Other123")), &imported),
              CredentialFile::Result::WRONG_PASSWORD);

    ASSERT_EQ(imported.size(), 1);
    EXPECT_EQ(imported.front().displayName(), "kept");
}

//--------------------------------------------------------------------------------------------------
// A damaged file opened with the right password is not blamed on the password.
TEST_F(CredentialFileTest, DamagedFileIsNotAWrongPassword)
{
    ASSERT_TRUE(dir_.isValid());
    ASSERT_EQ(CredentialFile::exportToFile({ makeCredential("name", "user", "secret") }, filePath(),
                                           password()),
              CredentialFile::Result::SUCCESS);

    QFile file(filePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    proto::storage::CredentialFile file_message;
    ASSERT_TRUE(parse(file.readAll(), &file_message));
    file.close();

    std::string data = file_message.data();
    data.back() = static_cast<char>(data.back() ^ 0x01);
    file_message.set_data(data);
    ASSERT_TRUE(writeFile(filePath(), serialize(file_message)));

    QList<CredentialConfig> imported;
    EXPECT_EQ(CredentialFile::importFromFile(filePath(), password(), &imported),
              CredentialFile::Result::INVALID_FORMAT);
}

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialFileTest, MissingFileIsAFileError)
{
    ASSERT_TRUE(dir_.isValid());

    QList<CredentialConfig> imported;
    EXPECT_EQ(CredentialFile::importFromFile(filePath(), password(), &imported),
              CredentialFile::Result::FILE_ERROR);
}

//--------------------------------------------------------------------------------------------------
// A record whose sealed data did not open comes without a user name and a password.
TEST_F(CredentialFileTest, UnreadableRecordIsNotExported)
{
    ASSERT_TRUE(dir_.isValid());

    CredentialConfig unreadable = makeCredential("name", "user", "secret");
    unreadable.setUsername(QString());
    unreadable.setPassword(SecureString());

    EXPECT_EQ(CredentialFile::exportToFile({ makeCredential("good", "user", "secret"), unreadable },
                                           filePath(), password()),
              CredentialFile::Result::UNREADABLE_RECORD);
    EXPECT_FALSE(QFile::exists(filePath()));
}

//--------------------------------------------------------------------------------------------------
TEST_F(CredentialFileTest, FileWithInvalidRecordIsRefused)
{
    ASSERT_TRUE(dir_.isValid());

    const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    proto::storage::CredentialFile::Content content;
    for (int i = 0; i < 2; ++i)
    {
        proto::storage::CredentialFile::Credential* credential = content.add_credentials();
        credential->set_guid(guid.toStdString());
        credential->set_display_name("name");
        credential->set_username("user");
        credential->set_password("secret");
    }

    ASSERT_TRUE(writeContent(content));

    QList<CredentialConfig> imported;
    EXPECT_EQ(CredentialFile::importFromFile(filePath(), password(), &imported),
              CredentialFile::Result::INVALID_FORMAT);
    EXPECT_TRUE(imported.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A backup of the address book is sealed the same way, and a credentials file must not take it.
TEST_F(CredentialFileTest, BackupIsNotACredentialsFile)
{
    ASSERT_TRUE(dir_.isValid());

    proto::storage::BackupFile backup;
    backup.set_version(1);
    backup.set_salt(Random::byteArray(32).toStdString());
    backup.set_verifier(Random::byteArray(60).toStdString());
    backup.set_data(Random::byteArray(100).toStdString());
    ASSERT_TRUE(writeFile(filePath(), serialize(backup)));

    QList<CredentialConfig> imported;
    EXPECT_EQ(CredentialFile::importFromFile(filePath(), password(), &imported),
              CredentialFile::Result::INVALID_FORMAT);
}

//--------------------------------------------------------------------------------------------------
// The import of a backup replaces the whole address book, so it must not take a credentials file.
TEST_F(CredentialFileTest, CredentialsFileIsNotABackup)
{
    ASSERT_TRUE(dir_.isValid());
    ASSERT_EQ(CredentialFile::exportToFile({ makeCredential("name", "user", "secret") }, filePath(),
                                           password()),
              CredentialFile::Result::SUCCESS);

    Database db;
    ASSERT_TRUE(db.open(dir_.filePath("book.db3")));

    EXPECT_EQ(Backup::importFromFile(db, filePath(), password()), Backup::Result::INVALID_FORMAT);
}
