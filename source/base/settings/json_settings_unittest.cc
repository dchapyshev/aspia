//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/settings/json_settings.h"

#include <gtest/gtest.h>

namespace base {

namespace {

bool removeFile(const std::filesystem::path& file_path)
{
    std::error_code ignored_code;

    if (!std::filesystem::remove(file_path, ignored_code))
        return false;

    return std::filesystem::remove(file_path.parent_path(), ignored_code);
}

} // namespace

TEST(JsonSettingsTest, SettingsTest)
{
    static const base::ByteArray kSeedKey = base::fromHex("0FB4A836124156ABFF4E1212");

    struct TestUser
    {
        const char* name;
        const char* salt;
        uint32_t flags;
    } const kUserList[] =
    {
        { "user1", "0FB4A8361241", 0 },
        { "user2", "1FB4A8361242", 7 },
        { "user3", "2FB4A8361243", 16 }
    };

    std::unique_ptr<JsonSettings> settings =
        std::make_unique<JsonSettings>(JsonSettings::Scope::USER, "test", "temp.json");

    settings->set<uint32_t>("TcpPort", 8050);
    settings->set<base::ByteArray>("SeedKey", kSeedKey);

    base::Settings::Array test_array;

    for (size_t i = 0; i < std::size(kUserList); ++i)
    {
        base::Settings item;

        item.set<std::string>("Name", kUserList[i].name);
        item.set<base::ByteArray>("Salt", base::fromHex(kUserList[i].salt));
        item.set<uint32_t>("Flags", kUserList[i].flags);

        test_array.emplace_back(std::move(item));
    }

    settings->setArray("Users", test_array);

    EXPECT_EQ(settings->get<uint32_t>("TcpPort", 0), 8050);
    EXPECT_EQ(settings->get<base::ByteArray>("SeedKey", base::ByteArray()), kSeedKey);

    test_array = settings->getArray("Users");

    for (size_t i = 0; i < std::size(kUserList); ++i)
    {
        EXPECT_EQ(test_array[i].get<std::string>("Name", std::string()), kUserList[i].name);
        EXPECT_EQ(test_array[i].get<base::ByteArray>("Salt", base::ByteArray()), base::fromHex(kUserList[i].salt));
        EXPECT_EQ(test_array[i].get<uint32_t>("Flags", -1), kUserList[i].flags);
    }

    settings.reset();

    // Re-open settings file.
    settings = std::make_unique<JsonSettings>(JsonSettings::Scope::USER, "test", "temp.json");

    EXPECT_EQ(settings->get<uint32_t>("TcpPort", 0), 8050);
    EXPECT_EQ(settings->get<base::ByteArray>("SeedKey", base::ByteArray()), kSeedKey);

    test_array = settings->getArray("Users");
    EXPECT_FALSE(test_array.empty());

    if (!test_array.empty())
    {
        for (size_t i = 0; i < std::size(kUserList); ++i)
        {
            EXPECT_EQ(test_array[i].get<std::string>("Name", std::string()), kUserList[i].name);
            EXPECT_EQ(test_array[i].get<base::ByteArray>("Salt", base::ByteArray()), base::fromHex(kUserList[i].salt));
            EXPECT_EQ(test_array[i].get<uint32_t>("Flags", -1), kUserList[i].flags);
        }
    }

    std::filesystem::path file_path = settings->filePath();
    settings.reset();

    bool ret = removeFile(file_path);
    EXPECT_TRUE(ret);
}

TEST(JsonSettingsTest, DISABLED_Performance)
{
    std::unique_ptr<JsonSettings> settings =
        std::make_unique<JsonSettings>(JsonSettings::Scope::USER, "test", "temp.json");

    for (size_t i = 0; i < 1000000; ++i)
    {
        settings->set<bool>("key1", true);

        Settings group1;
        group1.set<bool>("value1", true);
        group1.set<bool>("value2", true);

        settings->setGroup("group", group1);

        Settings group2 = settings->getGroup("group");
        EXPECT_TRUE(group2.get<bool>("value1"));
        EXPECT_TRUE(group2.get<bool>("value2"));

        EXPECT_TRUE(settings->get<bool>("key1"));

        settings->remove("group");
    }

    std::filesystem::path file_path = settings->filePath();
    settings.reset();

    bool ret = removeFile(file_path);
    EXPECT_TRUE(ret);
}

} // namespace base
