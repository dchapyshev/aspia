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

#include "client/desktop/management/profile_combo_box.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "client/router/router_controller.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/profile_store.h"

class ProfileComboBoxTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        DatabaseTestPeer::setFilePath(dir_.filePath("client.db3"));
        ASSERT_TRUE(Database::instance().isValid());
    }

    static qint64 addProfile(const QString& name)
    {
        ProfileConfig profile;
        profile.setName(name);

        EXPECT_TRUE(ProfileStore().add(profile));
        return profile.id();
    }

    QTemporaryDir dir_;
    RouterController controller_;
};

//--------------------------------------------------------------------------------------------------
// The default profile comes first with the id 0 and is chosen until the screen chooses another one.
TEST_F(ProfileComboBoxTest, DefaultProfileIsOfferedFirst)
{
    const qint64 zeta_id = addProfile("zeta");
    const qint64 alpha_id = addProfile("alpha");

    ProfileComboBox combo;
    ASSERT_TRUE(combo.onReload());

    ASSERT_EQ(combo.count(), 3);
    EXPECT_EQ(combo.itemData(0).toLongLong(), 0);
    EXPECT_EQ(combo.itemData(1).toLongLong(), alpha_id);
    EXPECT_EQ(combo.itemData(2).toLongLong(), zeta_id);
    EXPECT_EQ(combo.currentProfileId(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ProfileComboBoxTest, ChoiceIsKeptWhenTheListIsReadAgain)
{
    addProfile("first");
    const qint64 second_id = addProfile("second");

    ProfileComboBox combo;
    ASSERT_TRUE(combo.onReload());

    combo.setCurrentProfileId(second_id);
    ASSERT_EQ(combo.currentProfileId(), second_id);

    addProfile("another");
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.count(), 4);
    EXPECT_EQ(combo.currentProfileId(), second_id);
}

//--------------------------------------------------------------------------------------------------
// A host always runs on some profile, so the default one takes the place of a profile that is gone.
TEST_F(ProfileComboBoxTest, ChoiceFallsBackToDefaultOnceTheProfileIsGone)
{
    addProfile("first");
    const qint64 second_id = addProfile("second");

    ProfileComboBox combo;
    ASSERT_TRUE(combo.onReload());

    combo.setCurrentProfileId(second_id);
    ASSERT_EQ(combo.currentProfileId(), second_id);

    ASSERT_TRUE(ProfileStore().remove(second_id));
    ASSERT_TRUE(combo.onReload());

    EXPECT_EQ(combo.count(), 2);
    EXPECT_EQ(combo.currentProfileId(), 0);

    combo.setCurrentProfileId(second_id);
    EXPECT_EQ(combo.currentProfileId(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(ProfileComboBoxTest, ListIsReadAgainWhenProfilesChange)
{
    ProfileComboBox combo;
    ASSERT_TRUE(combo.onReload());
    ASSERT_EQ(combo.count(), 1);

    const qint64 profile_id = addProfile("office");
    emit controller_.sig_profilesChanged();

    ASSERT_EQ(combo.count(), 2);
    EXPECT_EQ(combo.itemData(1).toLongLong(), profile_id);
}
