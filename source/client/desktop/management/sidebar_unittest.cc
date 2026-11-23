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

#include "client/desktop/management/sidebar.h"

#include <QTemporaryDir>
#include <QTreeWidgetItemIterator>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "client/router/router_controller.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/local_group_store.h"

class SidebarTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        DatabaseTestPeer::setFilePath(dir_.filePath("client.db3"));
        ASSERT_TRUE(Database::instance().isValid());
    }

    static qint64 addGroup(const QString& name, qint64 parent_id)
    {
        LocalGroupConfig group;
        group.setName(name);
        group.setParentId(parent_id);

        EXPECT_TRUE(LocalGroupStore().add(group));
        return group.id();
    }

    static SidebarItem* groupItem(const Sidebar& sidebar, qint64 group_id)
    {
        for (QTreeWidgetItemIterator it(sidebar.findChild<QTreeWidget*>()); *it; ++it)
        {
            SidebarItem* item = static_cast<SidebarItem*>(*it);
            if (item->itemType() == SidebarItem::LOCAL_GROUP && item->groupId() == group_id)
                return item;
        }

        return nullptr;
    }

    QTemporaryDir dir_;
    RouterController controller_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(SidebarTest, ReceivedGroupAppearsBelowItsParent)
{
    const qint64 office_id = addGroup("office", 0);

    Sidebar sidebar;
    ASSERT_NE(groupItem(sidebar, office_id), nullptr);

    const qint64 servers_id = addGroup("servers", office_id);
    emit controller_.sig_localGroupsChanged();

    SidebarItem* servers_item = groupItem(sidebar, servers_id);
    ASSERT_NE(servers_item, nullptr);
    EXPECT_EQ(servers_item->parent(), groupItem(sidebar, office_id));
}

//--------------------------------------------------------------------------------------------------
// A group that is renamed and moved keeps its item, so the group shown is not opened anew.
TEST_F(SidebarTest, ChangedGroupKeepsItsItem)
{
    const qint64 office_id = addGroup("office", 0);
    const qint64 home_id = addGroup("home", 0);
    const qint64 servers_id = addGroup("servers", office_id);

    Sidebar sidebar;

    SidebarItem* servers_item = groupItem(sidebar, servers_id);
    ASSERT_NE(servers_item, nullptr);

    sidebar.findChild<QTreeWidget*>()->setCurrentItem(servers_item);
    ASSERT_EQ(sidebar.currentGroupId(), servers_id);

    int switch_count = 0;
    QObject::connect(&sidebar, &Sidebar::sig_switchContent, &sidebar, [&switch_count]() { ++switch_count; });

    LocalGroupConfig servers;
    ASSERT_EQ(LocalGroupStore().find(servers_id, &servers), Database::FindResult::FOUND);

    servers.setName("machines");
    servers.setParentId(home_id);
    ASSERT_TRUE(LocalGroupStore().modify(servers));

    emit controller_.sig_localGroupsChanged();

    EXPECT_EQ(groupItem(sidebar, servers_id), servers_item);
    EXPECT_EQ(servers_item->text(0), "machines");
    EXPECT_EQ(servers_item->parent(), groupItem(sidebar, home_id));
    EXPECT_EQ(sidebar.currentItem(), servers_item);
    EXPECT_EQ(sidebar.currentGroupId(), servers_id);
    EXPECT_EQ(switch_count, 0);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SidebarTest, CurrentGroupThatIsGoneGivesPlaceToTheGroupAboveIt)
{
    const qint64 office_id = addGroup("office", 0);
    const qint64 servers_id = addGroup("servers", office_id);
    const qint64 racks_id = addGroup("racks", servers_id);

    Sidebar sidebar;

    SidebarItem* racks_item = groupItem(sidebar, racks_id);
    ASSERT_NE(racks_item, nullptr);

    sidebar.findChild<QTreeWidget*>()->setCurrentItem(racks_item);
    ASSERT_EQ(sidebar.currentGroupId(), racks_id);

    int switch_count = 0;
    QObject::connect(&sidebar, &Sidebar::sig_switchContent, &sidebar, [&switch_count]() { ++switch_count; });

    ASSERT_TRUE(LocalGroupStore().remove(servers_id));
    emit controller_.sig_localGroupsChanged();

    EXPECT_EQ(groupItem(sidebar, servers_id), nullptr);
    EXPECT_EQ(groupItem(sidebar, racks_id), nullptr);
    EXPECT_EQ(sidebar.currentItem(), groupItem(sidebar, office_id));
    EXPECT_EQ(sidebar.currentGroupId(), office_id);
    EXPECT_EQ(switch_count, 1);
}
