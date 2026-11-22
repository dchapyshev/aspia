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

#include "client/desktop/management/router_host_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/net/udp_channel.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "client/config.h"
#include "client/router/router_controller.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/credential_store.h"
#include "client/storage/profile_store.h"
#include "client/storage/router_host_store.h"

class RouterHostDialogTestPeer
{
public:
    static bool isLoaded(const RouterHostDialog& dialog) { return dialog.credentials_loaded_; }

    // What the Ok button does with the credentials and the profile of the host. The button itself
    // needs a session with the router, which this stand does not have.
    static bool save(RouterHostDialog& dialog) { return dialog.saveCredentials() && dialog.saveProfile(); }
};

namespace {

const HostId kHostId = 100500;

// The dialog reads and writes the database of the test thread. The controller stands here without a
// worker and without a session, so the dialog asks the router for nothing.
class RouterHostDialogTest : public testing::Test
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

    static RouterHost routerHost()
    {
        RouterHost host;
        host.host_id = kHostId;
        return host;
    }

    // A host with its own pair and the given links, the way it comes from the router: with a
    // revision and with nothing left to send.
    void addReceivedHost(qint64 credential_id, qint64 profile_id)
    {
        RouterHostConfig host;
        host.setHostId(kHostId);
        host.setUsername("user");
        host.setPassword(SecureString(QString("secret")));
        host.setCredentialId(credential_id);
        ASSERT_TRUE(RouterHostStore().add(host));

        if (profile_id > 0)
            ASSERT_TRUE(RouterHostStore().setProfileId(kHostId, profile_id));

        markHostReceived(5);
    }

    void markHostReceived(qint64 revision)
    {
        SqlDatabase raw;
        ASSERT_TRUE(raw.open(file_path_));

        SqlQuery query(raw, "UPDATE router_hosts SET sync_revision=?, sync_dirty=0");
        query.addInt64(revision);
        ASSERT_TRUE(query.exec());
    }

    static qint64 addProfile(const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);
        profile.setDesktopConfig(defaultDesktopConfig());
        profile.setUdpMethods(UDP_METHOD_ALL);

        EXPECT_TRUE(ProfileStore().add(profile));
        return profile.id();
    }

    static RouterHostConfig storedHost()
    {
        RouterHostConfig host;
        EXPECT_EQ(RouterHostStore().find(kHostId, &host), Database::FindResult::FOUND);
        return host;
    }

    static qsizetype unsyncedHostCount()
    {
        QList<RouterHostStore::Unsynced> hosts;
        EXPECT_EQ(RouterHostStore().unsynced(&hosts), Database::ReadResult::OK);
        return hosts.size();
    }

    // Shows the dialog and lets it read the host into the form.
    static void load(RouterHostDialog& dialog)
    {
        dialog.show();

        for (int i = 0; i < 100 && !RouterHostDialogTestPeer::isLoaded(dialog); ++i)
            QCoreApplication::processEvents();

        ASSERT_TRUE(RouterHostDialogTestPeer::isLoaded(dialog));
    }

    QTemporaryDir dir_;
    QString file_path_;
    RouterController controller_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
// A host links a credential and a profile that have not come to the device yet, so the form shows
// neither. The user opens the dialog and accepts it without touching anything: the host stays as it
// is, with both links, and nothing is left to send.
TEST_F(RouterHostDialogTest, FormThatIsNotTouchedLeavesTheHostAsItIs)
{
    qint64 credential_id = 0;
    ASSERT_TRUE(CredentialStore().addEmptyRow("11111111-1111-1111-1111-111111111111", &credential_id));

    qint64 profile_id = 0;
    ASSERT_TRUE(ProfileStore().addEmptyRow("22222222-2222-2222-2222-222222222222", &profile_id));

    addReceivedHost(credential_id, profile_id);

    RouterHostDialog dialog(routerHost(), nullptr);
    load(dialog);

    auto* saved_credentials = dialog.findChild<QCheckBox*>("checkbox_saved_credentials");
    ASSERT_TRUE(saved_credentials);
    EXPECT_FALSE(saved_credentials->isChecked());

    ASSERT_TRUE(RouterHostDialogTestPeer::save(dialog));

    const RouterHostConfig stored = storedHost();
    EXPECT_EQ(stored.username(), QString("user"));
    EXPECT_EQ(stored.credentialId(), credential_id);
    EXPECT_EQ(stored.profileId(), profile_id);
    EXPECT_EQ(unsyncedHostCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host links a credential that has not come to the device yet. The user chooses a profile for
// the host and touches nothing else: the profile is saved, and the link to the credential stays.
TEST_F(RouterHostDialogTest, ChosenProfileIsSavedAndTheCredentialsAreLeft)
{
    qint64 credential_id = 0;
    ASSERT_TRUE(CredentialStore().addEmptyRow("11111111-1111-1111-1111-111111111111", &credential_id));

    const qint64 profile_id = addProfile("office");

    addReceivedHost(credential_id, 0);

    RouterHostDialog dialog(routerHost(), nullptr);
    load(dialog);

    auto* profile = dialog.findChild<QComboBox*>("combo_profile");
    ASSERT_TRUE(profile);

    QTest::keyClick(profile, Qt::Key_Down);
    ASSERT_EQ(profile->currentData().toLongLong(), profile_id);

    ASSERT_TRUE(RouterHostDialogTestPeer::save(dialog));

    const RouterHostConfig stored = storedHost();
    EXPECT_EQ(stored.profileId(), profile_id);
    EXPECT_EQ(stored.username(), QString("user"));
    EXPECT_EQ(stored.credentialId(), credential_id);
    EXPECT_EQ(unsyncedHostCount(), 1);
}

//--------------------------------------------------------------------------------------------------
// A host links a credential that has not come to the device yet, and the form shows the pair of
// the host. The user changes the pair: the host is saved the way the form shows it, with the pair
// and without the link.
TEST_F(RouterHostDialogTest, ChangedPairIsSavedTheWayTheFormShowsIt)
{
    qint64 credential_id = 0;
    ASSERT_TRUE(CredentialStore().addEmptyRow("11111111-1111-1111-1111-111111111111", &credential_id));

    addReceivedHost(credential_id, 0);

    RouterHostDialog dialog(routerHost(), nullptr);
    load(dialog);

    auto* username = dialog.findChild<QLineEdit*>("edit_username");
    ASSERT_TRUE(username);

    QTest::keyClicks(username, "2");

    ASSERT_TRUE(RouterHostDialogTestPeer::save(dialog));

    const RouterHostConfig stored = storedHost();
    EXPECT_EQ(stored.username(), QString("user2"));
    EXPECT_EQ(stored.password().toString(), QString("secret"));
    EXPECT_EQ(stored.credentialId(), 0);
    EXPECT_EQ(unsyncedHostCount(), 1);
}

//--------------------------------------------------------------------------------------------------
// The dialog is open, when another pair of the host comes from the router. The user accepts the
// dialog without touching anything: the pair that came stays, and nothing is left to send.
TEST_F(RouterHostDialogTest, ChangeThatCameWhileTheDialogIsOpenStaysWhenTheFormIsNotTouched)
{
    addReceivedHost(0, 0);

    RouterHostDialog dialog(routerHost(), nullptr);
    load(dialog);

    RouterHostConfig received;
    received.setHostId(kHostId);
    received.setUsername("laptop");
    received.setPassword(SecureString(QString("secret")));
    ASSERT_TRUE(RouterHostStore().modify(received));
    markHostReceived(6);

    ASSERT_TRUE(RouterHostDialogTestPeer::save(dialog));

    EXPECT_EQ(storedHost().username(), QString("laptop"));
    EXPECT_EQ(unsyncedHostCount(), 0);
}
