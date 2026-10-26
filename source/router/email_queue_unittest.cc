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

#include "router/email_queue.h"

#include <QTemporaryDir>

#include <gtest/gtest.h>

class EmailQueueTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(temp_dir_.isValid());
        file_path_ = temp_dir_.path() + "/email_queue.db3";
        ASSERT_TRUE(queue_.open(file_path_));
    }

    QTemporaryDir temp_dir_;
    QString file_path_;
    EmailQueue queue_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(EmailQueueTest, EmptyQueueHasNoFront)
{
    EmailQueue::Record record;
    EXPECT_FALSE(queue_.front(&record));
}

//--------------------------------------------------------------------------------------------------
TEST_F(EmailQueueTest, EmailsComeOutInTheOrderTheyWereAdded)
{
    const qint64 before = secondsSinceEpoch();
    ASSERT_TRUE(queue_.add("first@example.com", "First", "Text 1"));
    ASSERT_TRUE(queue_.add("second@example.com", "Second", "Текст 2"));

    EmailQueue::Record record;
    ASSERT_TRUE(queue_.front(&record));
    EXPECT_GE(record.time, before);
    EXPECT_LE(record.time, secondsSinceEpoch());
    EXPECT_EQ(record.to, "first@example.com");
    EXPECT_EQ(record.subject, "First");
    EXPECT_EQ(record.text, "Text 1");

    EmailQueue::Record again;
    ASSERT_TRUE(queue_.front(&again));
    EXPECT_EQ(again.id, record.id);

    ASSERT_TRUE(queue_.remove(record.id));
    ASSERT_TRUE(queue_.front(&record));
    EXPECT_NE(record.id, again.id);
    EXPECT_EQ(record.to, "second@example.com");
    EXPECT_EQ(record.subject, "Second");
    EXPECT_EQ(record.text, "Текст 2");

    ASSERT_TRUE(queue_.remove(record.id));
    EXPECT_FALSE(queue_.front(&record));
}

//--------------------------------------------------------------------------------------------------
TEST_F(EmailQueueTest, QueueSurvivesReopen)
{
    ASSERT_TRUE(queue_.add("user@example.com", "Subject", "Text"));

    queue_.close();
    EXPECT_FALSE(queue_.isValid());
    ASSERT_TRUE(queue_.open(file_path_));

    EmailQueue::Record record;
    ASSERT_TRUE(queue_.front(&record));
    EXPECT_EQ(record.to, "user@example.com");
    EXPECT_EQ(record.subject, "Subject");
    EXPECT_EQ(record.text, "Text");
}

//--------------------------------------------------------------------------------------------------
TEST_F(EmailQueueTest, ClosedQueueRefusesEverything)
{
    queue_.close();

    EmailQueue::Record record;
    EXPECT_FALSE(queue_.add("user@example.com", "Subject", "Text"));
    EXPECT_FALSE(queue_.front(&record));
    EXPECT_FALSE(queue_.remove(1));
}
