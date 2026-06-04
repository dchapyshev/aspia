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

#include "base/sqlite/database.h"
#include "base/sqlite/statement.h"
#include "base/sqlite/transaction.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>

namespace {

//--------------------------------------------------------------------------------------------------
bool createSchema(sqlite::Database& db)
{
    return db.execute("CREATE TABLE t ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "name TEXT NOT NULL,"
                      "data BLOB NOT NULL,"
                      "value INTEGER NOT NULL)");
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, OpenInMemory)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    EXPECT_TRUE(db.isOpen());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, InsertAndReadBack)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        ASSERT_TRUE(insert.isValid());
        insert.addText(QString("host-1"))
              .addBlob(QByteArray("\x00\x01\x02", 3))
              .addInt64(42);
        EXPECT_TRUE(insert.execute());
    }

    EXPECT_EQ(db.lastInsertRowId(), 1);
    EXPECT_EQ(db.changes(), 1);

    sqlite::Statement select(db, "SELECT id, name, data, value FROM t");
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
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(std::string_view()).addBlob(QByteArray()).addInt64(0);
    ASSERT_TRUE(insert.execute());

    sqlite::Statement select(db, "SELECT name, data FROM t");
    ASSERT_TRUE(select.next());
    EXPECT_FALSE(select.columnIsNull(0));
    EXPECT_FALSE(select.columnIsNull(1));
    EXPECT_TRUE(select.columnTextView(0).empty());
    EXPECT_TRUE(select.columnBlobView(1).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, ResetReusesStatement)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(insert.reset());
        insert.addText(QString("n%1").arg(i)).addBlob(QByteArray()).addInt64(i);
        ASSERT_TRUE(insert.execute());
    }

    sqlite::Statement count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 3);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, TransactionRollbackOnScopeExit)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        sqlite::Transaction transaction(db);
        ASSERT_TRUE(transaction.begin());

        sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        insert.addText(QString("temp")).addBlob(QByteArray()).addInt64(1);
        ASSERT_TRUE(insert.execute());
        // No commit() - the destructor must roll back.
    }

    sqlite::Statement count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, TransactionCommitPersists)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    {
        sqlite::Transaction transaction(db);
        ASSERT_TRUE(transaction.begin());

        sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
        insert.addText(QString("kept")).addBlob(QByteArray()).addInt64(1);
        ASSERT_TRUE(insert.execute());

        ASSERT_TRUE(transaction.commit());
    }

    sqlite::Statement count(db, "SELECT COUNT(*) FROM t");
    ASSERT_TRUE(count.next());
    EXPECT_EQ(count.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, UInt64RoundTrip)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    const quint64 big = 0xFFFFFFFFFFFFFFF0ull;

    sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(QString("u")).addBlob(QByteArray()).addUInt64(big);
    ASSERT_TRUE(insert.execute());

    sqlite::Statement select(db, "SELECT value FROM t");
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnUInt64(0), big);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, CaseFoldSearchCyrillic)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));
    ASSERT_TRUE(createSchema(db));

    sqlite::Statement insert(db, "INSERT INTO t (name, data, value) VALUES (?, ?, ?)");
    insert.addText(QString::fromUtf8("Сервер Бухгалтерии")).addBlob(QByteArray()).addInt64(0);
    ASSERT_TRUE(insert.execute());

    // A lower-case Cyrillic substring must match the mixed-case stored name once both sides are
    // folded - the stock ASCII-only LIKE would miss it.
    sqlite::Statement select(db, "SELECT COUNT(*) FROM t "
                                 "WHERE casefold(name) LIKE casefold(?) ESCAPE '\\'");
    select.addText(QString::fromUtf8("%бухгалтерии%"));
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnInt64(0), 1);
}

//--------------------------------------------------------------------------------------------------
TEST(SqliteTest, CaseFoldFunctionFoldsValue)
{
    sqlite::Database db;
    ASSERT_TRUE(db.open(":memory:"));

    sqlite::Statement select(db, "SELECT casefold(?)");
    select.addText(QString::fromUtf8("ПРИВЕТ World"));
    ASSERT_TRUE(select.next());
    EXPECT_EQ(select.columnText(0), QString::fromUtf8("привет world"));
}
