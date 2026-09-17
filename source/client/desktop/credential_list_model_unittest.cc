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

#include "client/desktop/credential_list_model.h"

#include <QAbstractItemModelTester>

#include <gtest/gtest.h>

#include <memory>

namespace {

using Column = CredentialListModel::Column;

// Where the tester of Qt reports what it found. It has no way to fail a test of gtest by itself,
// so its complaints are picked out of the messages of Qt.
QStringList* g_contract_failures = nullptr;
QtMessageHandler g_previous_handler = nullptr;

//--------------------------------------------------------------------------------------------------
void recordingMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (type == QtWarningMsg && g_contract_failures)
        g_contract_failures->append(msg);

    if (g_previous_handler)
        g_previous_handler(type, context, msg);
}

//--------------------------------------------------------------------------------------------------
CredentialConfig credential(qint64 id, const QString& name, const QString& username)
{
    CredentialConfig config;
    config.setId(id);
    config.setDisplayName(name);
    config.setUsername(username);
    config.setPassword(SecureString(QString("secret")));
    return config;
}

} // namespace

class CredentialListModelTest : public testing::Test
{
protected:
    void SetUp() override
    {
        g_contract_failures = &contract_failures_;
        g_previous_handler = qInstallMessageHandler(recordingMessageHandler);

        model_ = std::make_unique<CredentialListModel>();
        tester_ = std::make_unique<QAbstractItemModelTester>(
            model_.get(), QAbstractItemModelTester::FailureReportingMode::Warning);
    }

    void TearDown() override
    {
        // The tester says what it found while the model it watches is still there.
        tester_.reset();
        model_.reset();

        qInstallMessageHandler(g_previous_handler);
        g_previous_handler = nullptr;
        g_contract_failures = nullptr;

        EXPECT_TRUE(contract_failures_.isEmpty())
            << contract_failures_.join('\n').toStdString();
    }

    QStringList names() const
    {
        QStringList list;
        for (int row = 0; row < model_->rowCount(); ++row)
            list.append(model_->data(model_->index(row, static_cast<int>(Column::NAME)),
                                     Qt::DisplayRole).toString());
        return list;
    }

    std::unique_ptr<CredentialListModel> model_;

private:
    std::unique_ptr<QAbstractItemModelTester> tester_;
    QStringList contract_failures_;
};

//--------------------------------------------------------------------------------------------------
// The sort the user picked holds through a new list.
TEST_F(CredentialListModelTest, SortIsKeptThroughReload)
{
    model_->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);
    model_->setCredentials({ credential(1, "host10", "admin"),
                             credential(2, "host2", "admin"),
                             credential(3, "Alpha", "root") });

    // Compared the way the user reads them, so "host2" comes before "host10".
    EXPECT_EQ(names(), QStringList({ "Alpha", "host2", "host10" }));

    model_->setCredentials({ credential(4, "host3", "admin"), credential(5, "Beta", "root") });
    EXPECT_EQ(names(), QStringList({ "Beta", "host3" }));

    model_->sort(static_cast<int>(Column::NAME), Qt::DescendingOrder);
    EXPECT_EQ(names(), QStringList({ "host3", "Beta" }));
}

//--------------------------------------------------------------------------------------------------
// A record is found by its id, and a row or an id that is not there answers nothing.
TEST_F(CredentialListModelTest, RowOfFindsRecords)
{
    model_->setCredentials({ credential(1, "Office", "admin"), credential(2, "Warehouse", "operator") });

    EXPECT_EQ(model_->rowOf(2), 1);
    ASSERT_NE(model_->credentialAt(1), nullptr);
    EXPECT_EQ(model_->credentialAt(1)->id(), 2);

    EXPECT_EQ(model_->credentialAt(5), nullptr);
    EXPECT_EQ(model_->rowOf(100500), -1);
}

//--------------------------------------------------------------------------------------------------
// A selection is an index the view keeps, and a sort moves it with the record.
TEST_F(CredentialListModelTest, PersistentIndexFollowsItsRecordThroughASort)
{
    model_->setCredentials({ credential(1, "Beta", "admin"), credential(2, "Alpha", "admin") });

    const QPersistentModelIndex selected = model_->index(0, 0);
    ASSERT_EQ(model_->credentialAt(selected.row())->id(), 1);

    model_->sort(static_cast<int>(Column::NAME), Qt::AscendingOrder);

    ASSERT_TRUE(selected.isValid());
    EXPECT_EQ(selected.row(), 1);
    EXPECT_EQ(model_->credentialAt(selected.row())->id(), 1);
}
