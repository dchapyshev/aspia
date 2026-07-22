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

#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>
#include <QTemporaryDir>

namespace {

//--------------------------------------------------------------------------------------------------
bool createSchema(SqlDatabase& db)
{
    return db.exec("CREATE TABLE t ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "name TEXT NOT NULL,"
                      "data BLOB NOT NULL,"
                      "value INTEGER NOT NULL)");
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, OpenInMemory)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    EXPECT_TRUE(db.isOpen());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, InsertAndReadBack)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        ASSERT_TRUE(insert.isValid());
        insert.addText(QString("host-1"))
              .addBlob(QByteArray("\x00\x01\x02", 3))
              .addInt64(42);
        EXPECT_TRUE(insert.exec());
    }

    EXPECT_EQ(db.lastInsertRowId(), 1);
    EXPECT_EQ(db.changes(), 1);

    SqlQuery select(db, "SELECT id, name, data, value FROM t");
    ASSERT_TRUE(select.isValid());
    ASSERT_TRUE(select.next());

    EXPECT_EQ(select.columnInt64(0), 1);
    EXPECT_EQ(select.columnTextView(1), std::string_view("host-1"));
    EXPECT_EQ(select.columnBlobView(2), std::string_view("\x00\x01\x02", 3));
    EXPECT_EQ(select.columnInt64(3), 42);

    EXPECT_FALSE(select.next());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, EmptyTextIsNotNull)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(std::string_view()).addBlob(QByteArray()).addInt64(0);
    ASSERT_TRUE(insert.exec());

    SqlQuery select(db, "SELECT name, data FROM t");
    ASSERT_TRUE(select.next());
    EXPECT_FALSE(select.columnIsNull(0));
    EXPECT_FALSE(select.columnIsNull(1));
    EXPECT_TRUE(select.columnTextView(0).empty());
    EXPECT_TRUE(select.columnBlobView(1).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, BlobFromStringView)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    const std::string_view payload("\x00\x10\x20\x00\x30", 5);

    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(std::string_view()).addBlob(payload).addInt64(0);
    ASSERT_TRUE(insert.exec());

    // An empty view must still bind an empty blob, not NULL.
    insert.reset();
    insert.addText(std::string_view()).addBlob(std::string_view()).addInt64(1);
    ASSERT_TRUE(insert.exec());

    SqlQuery select(db, "SELECT data FROM t ORDER BY value");
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnBlobView(0), payload);
    ASSERT_TRUE(select.next());
    EXPECT_FALSE(select.columnIsNull(0));
    EXPECT_TRUE(select.columnBlobView(0).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, BlobFromNullPointer)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    // A null pointer must bind an empty blob, not NULL - the NOT NULL column would reject the
    // row otherwise.
    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    ASSERT_TRUE(insert.bindText(1, std::string_view()));
    ASSERT_TRUE(insert.bindBlob(2, nullptr, 0));
    ASSERT_TRUE(insert.bindInt64(3, 0));
    ASSERT_TRUE(insert.exec());

    SqlQuery select(db, "SELECT data FROM t");
    ASSERT_TRUE(select.next());
    EXPECT_FALSE(select.columnIsNull(0));
    EXPECT_TRUE(select.columnBlobView(0).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, ResetReusesStatement)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(insert.reset());
        insert.addText(QString("n%1").arg(i)).addBlob(QByteArray()).addInt64(i);
        ASSERT_TRUE(insert.exec());
    }

    SqlQuery count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 3);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, TransactionRollbackOnScopeExit)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        SqlTransaction transaction(db);
        ASSERT_TRUE(transaction.begin());

        SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        insert.addText(QString("temp")).addBlob(QByteArray()).addInt64(1);
        ASSERT_TRUE(insert.exec());
        // No commit() - the destructor must roll back.
    }

    SqlQuery count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, TransactionCommitPersists)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        SqlTransaction transaction(db);
        ASSERT_TRUE(transaction.begin());

        SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        insert.addText(QString("kept")).addBlob(QByteArray()).addInt64(1);
        ASSERT_TRUE(insert.exec());

        ASSERT_TRUE(transaction.commit());
    }

    SqlQuery count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, FailedCommitDoesNotWedgeConnection)
{
    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());

    // Zero busy timeout: the test expects the lock collision to fail instantly, not after a wait.
    SqlDatabase writer;
    ASSERT_TRUE(writer.open(temp_dir.filePath("test.db"), Milliseconds(0)));
    ASSERT_TRUE(createSchema(writer));

    SqlDatabase reader;
    ASSERT_TRUE(reader.open(temp_dir.filePath("test.db"), Milliseconds(0)));

    // The reader holds a shared lock for the duration of its transaction, so the writer's COMMIT
    // (which needs an exclusive lock) fails with SQLITE_BUSY and leaves the transaction open.
    ASSERT_TRUE(reader.beginTransaction());
    {
        SqlQuery select(reader, "SELECT COUNT(*) FROM t");
        ASSERT_TRUE(select.next());
    }

    {
        SqlTransaction transaction(writer);
        ASSERT_TRUE(transaction.begin());

        SqlQuery insert(writer, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        insert.addText(QString("wedge")).addBlob(QByteArray()).addInt64(1);
        ASSERT_TRUE(insert.exec());

        EXPECT_FALSE(transaction.commit());
    }

    ASSERT_TRUE(reader.rollbackTransaction());

    // A failed commit must not leave the connection inside the old transaction: the next
    // transaction on it must begin and commit normally.
    SqlTransaction transaction(writer);
    EXPECT_TRUE(transaction.begin());
    EXPECT_TRUE(transaction.commit());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, ImmediateTransactionTakesWriteLock)
{
    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());

    // Zero busy timeout: the test expects the lock collision to fail instantly, not after a wait.
    SqlDatabase first;
    ASSERT_TRUE(first.open(temp_dir.filePath("test.db"), Milliseconds(0)));
    ASSERT_TRUE(createSchema(first));

    SqlDatabase second;
    ASSERT_TRUE(second.open(temp_dir.filePath("test.db"), Milliseconds(0)));

    // IMMEDIATE takes the write lock at BEGIN, so a competing immediate begin fails up front
    // instead of at the first write statement.
    SqlTransaction holder(first);
    ASSERT_TRUE(holder.begin(SqlTransaction::Mode::IMMEDIATE));

    SqlTransaction competitor(second);
    EXPECT_FALSE(competitor.begin(SqlTransaction::Mode::IMMEDIATE));

    ASSERT_TRUE(holder.commit());
    EXPECT_TRUE(competitor.begin(SqlTransaction::Mode::IMMEDIATE));
    EXPECT_TRUE(competitor.commit());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, UInt64RoundTrip)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    const quint64 big = 0xFFFFFFFFFFFFFFF0ull;

    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(QString("u")).addBlob(QByteArray()).addUInt64(big);
    ASSERT_TRUE(insert.exec());

    SqlQuery select(db, "SELECT value FROM t");
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnUInt64(0), big);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, CaseFoldSearchCyrillic)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    SqlQuery insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(QString::fromUtf8("Сервер Бухгалтерии")).addBlob(QByteArray()).addInt64(0);
    ASSERT_TRUE(insert.exec());

    // A lower-case Cyrillic substring must match the mixed-case stored name once both sides are
    // folded - the stock ASCII-only LIKE would miss it.
    SqlQuery select(db, "SELECT COUNT(*) FROM t "
                                 "WHERE casefold(name) LIKE casefold(?) ESCAPE '\\'");
    select.addText(QString::fromUtf8("%бухгалтерии%"));
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, CaseFoldFunctionFoldsValue)
{
    SqlDatabase db;
    ASSERT_TRUE(db.open(":memory:"));

    SqlQuery select(db, "SELECT casefold(?)");
    select.addText(QString::fromUtf8("ПРИВЕТ World"));
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnText(0), QString::fromUtf8("привет world"));
}
