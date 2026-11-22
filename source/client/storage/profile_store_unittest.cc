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

#include "client/storage/profile_store.h"

#include <QDateTime>

#include "base/serialization.h"
#include "client/storage/database_test_base.h"

using ProfileStoreTest = DatabaseTestBase;

//--------------------------------------------------------------------------------------------------
// The same for a profile. The default profile comes with the database and nobody edited it yet.
TEST_F(ProfileStoreTest, EditMarksTheMomentAProfileChanged)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();

    ProfileConfig default_profile = profiles_.defaultProfile();
    EXPECT_EQ(modifyTime("profiles", "id", default_profile.id()), 0);

    ASSERT_TRUE(profiles_.modify(default_profile));
    EXPECT_GE(modifyTime("profiles", "id", default_profile.id()), start_time);

    const qint64 profile_id = addProfile("fast");
    EXPECT_GE(modifyTime("profiles", "id", profile_id), start_time);

    ASSERT_TRUE(setModifyTime("profiles", "id", profile_id, 1));

    ProfileConfig edited;
    ASSERT_EQ(profiles_.find(profile_id, &edited), Database::FindResult::FOUND);
    edited.setName("slow");
    ASSERT_TRUE(profiles_.modify(edited));

    EXPECT_GE(modifyTime("profiles", "id", profile_id), start_time);
}

//--------------------------------------------------------------------------------------------------
// The default profile comes with the database: the built-in parameters until the user changes
// them, a change is kept. It is not one of the profiles the user made, and it cannot be deleted.
TEST_F(ProfileStoreTest, DefaultProfileComesWithTheDatabase)
{
    ProfileConfig profile = profiles_.defaultProfile();
    EXPECT_GT(profile.id(), 0);
    EXPECT_TRUE(profile.isDefault());
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(defaultDesktopConfig()));
    EXPECT_EQ(profile.udpMethods(), static_cast<quint32>(UDP_METHOD_ALL));
    EXPECT_FALSE(profile.desktopConfig().record_sessions());
    EXPECT_FALSE(recordingPath(profile.desktopConfig()).isEmpty());

    EXPECT_TRUE(profileList().isEmpty());

    addProfile("Aaa");

    QList<ProfileConfig> all;
    ASSERT_EQ(profiles_.list(&all), Database::ReadResult::OK);
    ASSERT_EQ(all.size(), 2);
    EXPECT_EQ(all.front().id(), profile.id());
    EXPECT_EQ(all.back().name(), QString("Aaa"));

    proto::control::Config config = defaultDesktopConfig();
    config.set_audio(false);
    config.set_block_input(true);
    config.set_record_sessions(true);
    config.set_recording_path("/records");
    profile.setDesktopConfig(config);
    profile.setUdpMethods(0);
    ASSERT_TRUE(profiles_.modify(profile));

    profile = profiles_.defaultProfile();
    EXPECT_EQ(serialize(profile.desktopConfig()), serialize(config));
    EXPECT_EQ(recordingPath(profile.desktopConfig()), "/records");
    EXPECT_EQ(profile.udpMethods(), 0u);

    EXPECT_FALSE(profiles_.remove(profile.id()));
    EXPECT_EQ(profiles_.defaultProfile().id(), profile.id());

    {
        Database reopened;
        ASSERT_TRUE(reopened.open(file_path_));

        ProfileStore store(reopened);
        EXPECT_EQ(store.defaultProfile().id(), profile.id());
        EXPECT_EQ(store.defaultProfile().udpMethods(), 0u);
    }
}

//--------------------------------------------------------------------------------------------------
// What comes back out of a profile has to be what went in, and every profile gets a guid of its own.
TEST_F(ProfileStoreTest, ProfileSurvivesARoundTrip)
{
    proto::control::Config desktop_config = defaultDesktopConfig();
    desktop_config.set_audio(false);
    desktop_config.set_quality(kHighQuality);
    desktop_config.set_record_sessions(true);
    desktop_config.set_recording_path("/records");

    ProfileConfig profile;
    profile.setName("office");
    profile.setDesktopConfig(desktop_config);
    profile.setUdpMethods(0);
    ASSERT_TRUE(profiles_.add(profile));
    EXPECT_GT(profile.id(), 0);
    EXPECT_FALSE(profile.guid().isEmpty());

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile.id(), &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.id(), profile.id());
    EXPECT_EQ(stored.guid(), profile.guid());
    EXPECT_EQ(stored.name(), QString("office"));
    EXPECT_EQ(serialize(stored.desktopConfig()), serialize(desktop_config));
    EXPECT_EQ(stored.udpMethods(), 0u);

    const qint64 other_id = addProfile("home");
    ProfileConfig other;
    ASSERT_EQ(profiles_.find(other_id, &other), Database::FindResult::FOUND);
    EXPECT_NE(other.guid(), stored.guid());

    ProfileConfig copy = stored;
    EXPECT_FALSE(profiles_.add(copy));

    ProfileConfig missing;
    EXPECT_EQ(profiles_.find(other_id + 1, &missing), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// An edit changes the name and the parameters, never the guid. A removed profile is gone.
TEST_F(ProfileStoreTest, ProfileIsEditedAndRemoved)
{
    const qint64 profile_id = addProfile("office");

    ProfileConfig before;
    ASSERT_EQ(profiles_.find(profile_id, &before), Database::FindResult::FOUND);

    proto::control::Config desktop_config = defaultDesktopConfig();
    desktop_config.set_disable_key_combinations(true);

    ProfileConfig edited;
    edited.setId(profile_id);
    edited.setName("renamed");
    edited.setDesktopConfig(desktop_config);
    edited.setUdpMethods(UDP_METHOD_DIRECT);
    EXPECT_TRUE(profiles_.modify(edited));

    ProfileConfig unknown = edited;
    unknown.setId(profile_id + 1);
    EXPECT_FALSE(profiles_.modify(unknown));

    ProfileConfig stored;
    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.guid(), before.guid());
    EXPECT_EQ(stored.name(), QString("renamed"));
    EXPECT_EQ(serialize(stored.desktopConfig()), serialize(desktop_config));
    EXPECT_EQ(stored.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(profileList().size(), 1);

    ASSERT_TRUE(profiles_.remove(profile_id));
    EXPECT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(profileList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Profiles come by name whatever order they were added in, and the case of a letter does not move
// them.
TEST_F(ProfileStoreTest, ProfilesAreListedByName)
{
    addProfile("office");
    addProfile("Home");
    addProfile(QString::fromUtf8("Склад"));
    addProfile("backup");

    const QList<ProfileConfig> profiles = profileList();
    ASSERT_EQ(profiles.size(), 4);
    EXPECT_EQ(profiles[0].name(), "backup");
    EXPECT_EQ(profiles[1].name(), "Home");
    EXPECT_EQ(profiles[2].name(), "office");
    EXPECT_EQ(profiles[3].name(), QString::fromUtf8("Склад"));
}

//--------------------------------------------------------------------------------------------------
// A profile is chosen by its name, so one without a name or with one too long is not stored.
TEST_F(ProfileStoreTest, ProfileWithoutANameIsNotStored)
{
    ProfileConfig nameless;
    EXPECT_FALSE(profiles_.add(nameless));

    ProfileConfig long_name;
    long_name.setName(QString(ProfileConfig::kMaxNameLength + 1, QChar('a')));
    EXPECT_FALSE(profiles_.add(long_name));

    EXPECT_TRUE(profileList().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A profile whose parameters do not read is in the list all the same, and the list says it is not
// whole.
TEST_F(ProfileStoreTest, ProfileThatDoesNotReadLeavesTheListIncomplete)
{
    const qint64 broken_id = addProfile("office");
    addProfile("home");

    ASSERT_TRUE(corruptRecordData("profiles", broken_id));

    QList<ProfileConfig> profiles;
    EXPECT_EQ(profiles_.list(&profiles), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(profiles.size(), 3);
    EXPECT_EQ(profiles[2].name(), QString("office"));
    EXPECT_EQ(profiles[2].id(), broken_id);

    ProfileConfig broken;
    EXPECT_EQ(profiles_.find(broken_id, &broken), Database::FindResult::UNREADABLE);
}

//--------------------------------------------------------------------------------------------------
// A new profile and an edit of one wait to be sent to the router, and every edit counts.
TEST_F(ProfileStoreTest, NewAndEditedProfilesAreUnsynced)
{
    const qint64 start_time = QDateTime::currentSecsSinceEpoch();
    const qint64 profile_id = addProfile("office");

    QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].profile.id(), profile_id);
    EXPECT_EQ(unsynced[0].profile.name(), QString("office"));
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_GE(unsynced[0].profile.modifyTime(), start_time);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);

    ASSERT_TRUE(markSynced("profiles", profile_id, 7));
    EXPECT_TRUE(unsyncedProfiles().isEmpty());

    ProfileConfig edited;
    ASSERT_EQ(profiles_.find(profile_id, &edited), Database::FindResult::FOUND);
    edited.setName("renamed");
    ASSERT_TRUE(profiles_.modify(edited));
    ASSERT_TRUE(profiles_.modify(edited));

    unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].profile.name(), QString("renamed"));
    EXPECT_FALSE(unsynced[0].deleted);
    EXPECT_EQ(unsynced[0].sync_revision, 7);
    EXPECT_EQ(unsynced[0].sync_dirty, 2);
}

//--------------------------------------------------------------------------------------------------
// The default profile nobody edited stays on the device, so it never replaces an edited one in the storage.
TEST_F(ProfileStoreTest, UntouchedDefaultProfileIsNotSentToTheStorage)
{
    const qint64 profile_id = addProfile("office");
    attachSyncStorage();

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_EQ(unsynced[0].profile.id(), profile_id);
    EXPECT_EQ(unsynced[0].sync_revision, 0);
}

//--------------------------------------------------------------------------------------------------
// An edited default profile goes to the storage like any other profile.
TEST_F(ProfileStoreTest, EditedDefaultProfileIsSentToTheStorage)
{
    ProfileConfig profile = profiles_.defaultProfile();
    profile.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(profiles_.modify(profile));
    ASSERT_TRUE(markSynced("profiles", profile.id(), 7));

    attachSyncStorage();

    const QList<ProfileStore::Unsynced> unsynced = unsyncedProfiles();
    ASSERT_EQ(unsynced.size(), 1);
    EXPECT_TRUE(unsynced[0].profile.isDefault());
    EXPECT_EQ(unsynced[0].profile.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(unsynced[0].sync_revision, 0);
    EXPECT_EQ(unsynced[0].sync_dirty, 1);
}

//--------------------------------------------------------------------------------------------------
// A profile the router has never held is gone the moment it is removed.
TEST_F(ProfileStoreTest, RemovedProfileTheRouterNeverHeldIsGoneAtOnce)
{
    const qint64 profile_id = addProfile("office");
    ASSERT_TRUE(profiles_.remove(profile_id));

    EXPECT_EQ(rowCount("profiles"), 1);
    EXPECT_TRUE(unsyncedProfiles().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An edit keeps the fields a newer version put into the profile, which this version does not know.
TEST_F(ProfileStoreTest, EditKeepsTheUnknownDataOfAProfile)
{
    attachSyncStorage();

    ProfileConfig received;
    received.setGuid("11111111-2222-4333-8444-555555555555");
    received.setName("office");
    received.setDesktopConfig(defaultDesktopConfig());
    received.setUdpMethods(UDP_METHOD_ALL);
    received.setUnknownData("unknown");
    ASSERT_TRUE(storeReceivedProfile(received, 7));

    ProfileConfig stored;
    ASSERT_EQ(profiles_.findByGuid(received.guid(), &stored), Database::FindResult::FOUND);
    const qint64 profile_id = stored.id();

    ProfileConfig edited;
    edited.setId(profile_id);
    edited.setName("renamed");
    edited.setDesktopConfig(defaultDesktopConfig());
    edited.setUdpMethods(UDP_METHOD_DIRECT);
    ASSERT_TRUE(profiles_.modify(edited));

    ASSERT_EQ(profiles_.find(profile_id, &stored), Database::FindResult::FOUND);
    EXPECT_EQ(stored.name(), QString("renamed"));
    EXPECT_EQ(stored.udpMethods(), static_cast<quint32>(UDP_METHOD_DIRECT));
    EXPECT_EQ(stored.unknownData(), QByteArray("unknown"));
}

//--------------------------------------------------------------------------------------------------
// The default profile stays whatever the user asks.
TEST_F(ProfileStoreTest, DefaultProfileIsNeverRemoved)
{
    const ProfileConfig profile = profiles_.defaultProfile();
    EXPECT_FALSE(profiles_.remove(profile.id()));

    EXPECT_EQ(profiles_.defaultProfile().id(), profile.id());
    EXPECT_EQ(rowCount("profiles"), 1);
}

//--------------------------------------------------------------------------------------------------
// The guid finds the profile and the default one, a removed profile is not found by it.
TEST_F(ProfileStoreTest, ProfileIsFoundByItsGuid)
{
    addRouter(true);
    ASSERT_EQ(router_store_.setSyncStorage(*findRouter(db_), Random::byteArray(16),
                                           SecureByteArray(Random::byteArray(32)), false),
              Database::SyncResult::DONE);

    const qint64 profile_id = addProfile("office");
    ProfileConfig added;
    ASSERT_EQ(profiles_.find(profile_id, &added), Database::FindResult::FOUND);

    ProfileConfig found;
    ASSERT_EQ(profiles_.findByGuid(added.guid(), &found), Database::FindResult::FOUND);
    EXPECT_EQ(found.id(), profile_id);
    EXPECT_EQ(found.name(), QString("office"));

    ASSERT_EQ(profiles_.findByGuid(ProfileConfig::kDefaultGuid, &found), Database::FindResult::FOUND);
    EXPECT_TRUE(found.isDefault());

    EXPECT_EQ(profiles_.findByGuid(QString(), &found), Database::FindResult::NOT_FOUND);

    ASSERT_TRUE(markSynced("profiles", profile_id, 7));
    ASSERT_TRUE(profiles_.remove(profile_id));
    ASSERT_TRUE(isMarkedDeleted("profiles", profile_id));
    EXPECT_EQ(profiles_.findByGuid(added.guid(), &found), Database::FindResult::NOT_FOUND);
}

//--------------------------------------------------------------------------------------------------
// A profile whose data does not read is listed among the unsynced ones as damaged.
TEST_F(ProfileStoreTest, UnsyncedProfileThatDoesNotReadIsDamaged)
{
    const qint64 broken_id = addProfile("office");
    addProfile("home");
    ASSERT_TRUE(corruptRecordData("profiles", broken_id));

    QList<ProfileStore::Unsynced> unsynced;
    ASSERT_EQ(profiles_.unsynced(&unsynced), Database::ReadResult::INCOMPLETE);
    ASSERT_EQ(unsynced.size(), 2);

    for (const ProfileStore::Unsynced& item : std::as_const(unsynced))
        EXPECT_EQ(item.damaged, item.profile.id() == broken_id);
}

//--------------------------------------------------------------------------------------------------
// The row of a profile is found by its guid whether or not the profile is marked as removed. A guid
// no row holds is not found.
TEST_F(ProfileStoreTest, RowIsFoundByGuidWhileTheProfileIsMarkedAsRemoved)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig added;
    ASSERT_EQ(profiles_.find(profile_id, &added), Database::FindResult::FOUND);

    qint64 row_id = 0;
    ASSERT_EQ(profiles_.findRowIdByGuid(added.guid(), &row_id), Database::FindResult::FOUND);
    EXPECT_EQ(row_id, profile_id);

    ASSERT_TRUE(markSynced("profiles", profile_id, 7));
    ASSERT_TRUE(profiles_.remove(profile_id));
    ASSERT_TRUE(isMarkedDeleted("profiles", profile_id));

    ASSERT_EQ(profiles_.findRowIdByGuid(added.guid(), &row_id), Database::FindResult::FOUND);
    EXPECT_EQ(row_id, profile_id);

    EXPECT_EQ(profiles_.findRowIdByGuid("22222222-2222-2222-2222-222222222222", &row_id),
              Database::FindResult::NOT_FOUND);
    EXPECT_EQ(row_id, 0);
}

//--------------------------------------------------------------------------------------------------
// The guid of a profile is found by the id of its row whether or not the profile is marked as
// removed. An id no row has is not found.
TEST_F(ProfileStoreTest, GuidIsFoundByRowIdWhileTheProfileIsMarkedAsRemoved)
{
    attachSyncStorage();

    const qint64 profile_id = addProfile("office");

    ProfileConfig added;
    ASSERT_EQ(profiles_.find(profile_id, &added), Database::FindResult::FOUND);

    QString guid;
    ASSERT_EQ(profiles_.findGuidByRowId(profile_id, &guid), Database::FindResult::FOUND);
    EXPECT_EQ(guid, added.guid());

    ASSERT_TRUE(markSynced("profiles", profile_id, 7));
    ASSERT_TRUE(profiles_.remove(profile_id));
    ASSERT_TRUE(isMarkedDeleted("profiles", profile_id));

    ASSERT_EQ(profiles_.findGuidByRowId(profile_id, &guid), Database::FindResult::FOUND);
    EXPECT_EQ(guid, added.guid());

    EXPECT_EQ(profiles_.findGuidByRowId(profile_id + 1, &guid), Database::FindResult::NOT_FOUND);
    EXPECT_TRUE(guid.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// An empty row holds the guid of a profile the device does not have yet, for the hosts that link it.
// The row is neither listed nor sent. The profile that comes with this guid takes the row and keeps
// its id.
TEST_F(ProfileStoreTest, EmptyRowIsTakenByTheProfileThatComesWithItsGuid)
{
    attachSyncStorage();

    const QString guid = "11111111-1111-1111-1111-111111111111";

    qint64 row_id = 0;
    ASSERT_TRUE(profiles_.addEmptyRow(guid, &row_id));
    EXPECT_GT(row_id, 0);
    EXPECT_TRUE(isMarkedDeleted("profiles", row_id));

    EXPECT_TRUE(profileList().isEmpty());
    EXPECT_TRUE(unsyncedProfiles().isEmpty());

    ProfileConfig found;
    EXPECT_EQ(profiles_.findByGuid(guid, &found), Database::FindResult::NOT_FOUND);

    qint64 found_id = 0;
    ASSERT_EQ(profiles_.findRowIdByGuid(guid, &found_id), Database::FindResult::FOUND);
    EXPECT_EQ(found_id, row_id);

    ProfileConfig received;
    received.setGuid(guid);
    received.setName("office");
    received.setDesktopConfig(defaultDesktopConfig());
    received.setUdpMethods(UDP_METHOD_ALL);
    received.setModifyTime(1000);
    ASSERT_TRUE(storeReceivedProfile(received, 12));

    ASSERT_EQ(profiles_.findByGuid(guid, &found), Database::FindResult::FOUND);
    EXPECT_EQ(found.id(), row_id);
    EXPECT_EQ(found.name(), QString("office"));
    EXPECT_EQ(profileList().size(), 1);
    EXPECT_TRUE(unsyncedProfiles().isEmpty());
}
