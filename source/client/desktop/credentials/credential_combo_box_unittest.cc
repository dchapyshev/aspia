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

#include "client/desktop/credentials/credential_combo_box.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/router/router_controller.h"
#include "client/router/router_test_fixture.h"

// The combo box reads the records from the database of the test thread and listens to the
// controller, which stands here without a worker and says of a change only when a test tells it to.
class CredentialComboBoxTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        file_path_ = dir_.filePath("client.db3");
        DatabaseTestPeer::setFilePath(file_path_);
        ASSERT_TRUE(Database::instance().isValid());
    }

    static qint64 addCredential(const QString& name, CredentialConfig::Type type = CredentialConfig::Type::ASPIA)
    {
        CredentialConfig credential;
        credential.setType(type);
        credential.setDisplayName(name);
        credential.setUsername("user");
        credential.setPassword(SecureString(QString("secret")));

        EXPECT_TRUE(Database::instance().addCredential(credential));
        return credential.id();
    }

    // Makes the record one that does not open.
    void damageCredential(qint64 credential_id)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path_));

        SqlQuery query(raw, "UPDATE credentials SET data=X'00' WHERE id=?");
        query.addInt64(credential_id);
        ASSERT_TRUE(query.exec());
    }

    QTemporaryDir dir_;
    QString file_path_;
    RouterController controller_;
};

//--------------------------------------------------------------------------------------------------
// The combo box offers the records of one type: the records a host of Aspia is entered with,
// unless it is told to offer the ones of an operating system.
TEST_F(CredentialComboBoxTest, RecordsOfTheChosenTypeAreOffered)
{
    const qint64 aspia_id = addCredential("office");
    const qint64 os_id = addCredential("server", CredentialConfig::Type::OS);

    CredentialComboBox combo;
    ASSERT_TRUE(combo.onReload());

    ASSERT_EQ(combo.count(), 1);
    EXPECT_EQ(combo.itemData(0).toLongLong(), aspia_id);

    CredentialComboBox os_combo;
    os_combo.setCredentialType(CredentialConfig::Type::OS);
    ASSERT_TRUE(os_combo.onReload());

    ASSERT_EQ(os_combo.count(), 1);
    EXPECT_EQ(os_combo.itemData(0).toLongLong(), os_id);
}

//--------------------------------------------------------------------------------------------------
// A record that did not open has no user name and no password to enter a host with, so it is
// left out. A screen that only links a host to a record asks for it to be shown.
TEST_F(CredentialComboBoxTest, UnreadableRecordIsOfferedOnlyWhenAskedFor)
{
    const qint64 good_id = addCredential("good");
    const qint64 damaged_id = addCredential("damaged");
    damageCredential(damaged_id);

    CredentialComboBox combo;
    ASSERT_TRUE(combo.onReload());

    ASSERT_EQ(combo.count(), 1);
    EXPECT_EQ(combo.itemData(0).toLongLong(), good_id);

    combo.setUnreadableShown(true);
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.count(), 2);
    EXPECT_GE(combo.findData(QVariant::fromValue(damaged_id)), 0);
}

//--------------------------------------------------------------------------------------------------
// Nothing is chosen until the screen chooses. What is chosen stays chosen when the list is read
// again, whatever else has come to the list.
TEST_F(CredentialComboBoxTest, ChoiceIsKeptWhenTheListIsReadAgain)
{
    addCredential("first");
    const qint64 second_id = addCredential("second");

    CredentialComboBox combo;
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.currentCredentialId(), 0);
    EXPECT_EQ(combo.currentCredential(), nullptr);

    combo.setCurrentCredentialId(second_id);
    ASSERT_EQ(combo.currentCredentialId(), second_id);

    addCredential("another");
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.count(), 3);
    EXPECT_EQ(combo.currentCredentialId(), second_id);

    const CredentialConfig* credential = combo.currentCredential();
    ASSERT_TRUE(credential);
    EXPECT_EQ(credential->displayName(), QString("second"));
}

//--------------------------------------------------------------------------------------------------
// A record that is gone cannot stay chosen, and no other record takes its place by itself.
TEST_F(CredentialComboBoxTest, ChoiceIsEmptyOnceTheRecordIsGone)
{
    addCredential("first");
    const qint64 second_id = addCredential("second");

    CredentialComboBox combo;
    ASSERT_TRUE(combo.onReload());

    combo.setCurrentCredentialId(second_id);
    ASSERT_EQ(combo.currentCredentialId(), second_id);

    ASSERT_TRUE(Database::instance().removeCredential(second_id));
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.count(), 1);
    EXPECT_EQ(combo.currentCredentialId(), 0);
    EXPECT_EQ(combo.currentCredential(), nullptr);
}

//--------------------------------------------------------------------------------------------------
// The combo box reads the list again by itself when it is told that the credentials have
// changed, and tells of it, so that the screen brings what depends on the list up to date.
TEST_F(CredentialComboBoxTest, ListIsReadAgainWhenCredentialsChange)
{
    CredentialComboBox combo;

    int reloaded_count = 0;
    QObject::connect(&combo, &CredentialComboBox::sig_reloaded, &combo, [&reloaded_count]()
    {
        ++reloaded_count;
    });

    ASSERT_TRUE(combo.onReload());
    ASSERT_EQ(combo.count(), 0);
    ASSERT_EQ(reloaded_count, 1);

    const qint64 credential_id = addCredential("office");
    emit controller_.sig_credentialsChanged();

    EXPECT_EQ(reloaded_count, 2);
    ASSERT_EQ(combo.count(), 1);
    EXPECT_EQ(combo.itemData(0).toLongLong(), credential_id);
}

//--------------------------------------------------------------------------------------------------
// Once the screen reads the list again, the combo box follows the changes again.
TEST_F(CredentialComboBoxTest, ClearedListIsNotReadAgainWhenCredentialsChange)
{
    addCredential("office");

    CredentialComboBox combo;

    int reloaded_count = 0;
    QObject::connect(&combo, &CredentialComboBox::sig_reloaded, &combo, [&reloaded_count]()
    {
        ++reloaded_count;
    });

    ASSERT_TRUE(combo.onReload());
    ASSERT_EQ(combo.count(), 1);

    combo.clearCredentials();
    EXPECT_EQ(combo.count(), 0);

    addCredential("server");
    emit controller_.sig_credentialsChanged();

    EXPECT_EQ(reloaded_count, 1);
    EXPECT_EQ(combo.count(), 0);

    ASSERT_TRUE(combo.onReload());
    EXPECT_EQ(combo.count(), 2);

    addCredential("home");
    emit controller_.sig_credentialsChanged();

    EXPECT_EQ(reloaded_count, 3);
    EXPECT_EQ(combo.count(), 3);
}
