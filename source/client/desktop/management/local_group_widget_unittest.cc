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

#include "client/desktop/management/local_group_widget.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "client/router/router_controller.h"
#include "client/router/router_test_fixture.h"
#include "client/storage/local_host_store.h"

class LocalGroupWidgetTest : public testing::Test
{
protected:
    void SetUp() override
    {
        DataCryptor::instance().setKey(SecureByteArray(Random::byteArray(32)));

        ASSERT_TRUE(dir_.isValid());

        DatabaseTestPeer::setFilePath(dir_.filePath("client.db3"));
        ASSERT_TRUE(Database::instance().isValid());
    }

    static qint64 addHost(const QString& name)
    {
        LocalHostConfig host;
        host.setName(name);
        host.setAddress("192.168.0.1");

        EXPECT_TRUE(LocalHostStore().add(host));
        return host.id();
    }

    QTemporaryDir dir_;
    RouterController controller_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(LocalGroupWidgetTest, ReceivedHostsReachTheShownGroupAndTheSelectionStays)
{
    const qint64 first_id = addHost("first");
    const qint64 second_id = addHost("second");

    LocalGroupWidget widget;
    widget.setOnlineCheckEnabled(false);
    widget.showGroup(0);
    widget.setCurrentHost(second_id);

    ASSERT_TRUE(LocalHostStore().remove(first_id));
    const qint64 third_id = addHost("third");

    emit controller_.sig_localHostsChanged();

    const LocalHostListModel* model = widget.findChild<LocalHostListModel*>();
    ASSERT_NE(model, nullptr);

    EXPECT_EQ(model->rowCount(), 2);
    EXPECT_EQ(model->rowOf(first_id), -1);
    EXPECT_GE(model->rowOf(third_id), 0);

    const QList<LocalHostConfig> selected_hosts = widget.selectedHosts();
    ASSERT_EQ(selected_hosts.size(), 1);
    EXPECT_EQ(selected_hosts.front().id(), second_id);
}
