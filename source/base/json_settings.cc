//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/json_settings.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/files/scoped_temp_file.h"
#include "base/strings/string_split.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include <fstream>

namespace base {

namespace {

std::string createKey(const std::vector<std::string_view>& segments)
{
    std::string key;

    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i != 0)
            key += Settings::kSeparator;
        key.append(segments.at(i));
    }

    return key;
}

template <class T>
void parseObject(const T& object, std::vector<std::string_view>* segments, Settings::Map* map)
{
    for (auto it = object.MemberBegin(); it != object.MemberEnd(); ++it)
    {
        const rapidjson::Value& name = it->name;
        const rapidjson::Value& value = it->value;

        segments->emplace_back(name.GetString(), name.GetStringLength());

        switch (value.GetType())
        {
            case rapidjson::kObjectType:
            {
                parseObject(value.GetObject(), segments, map);
            }
            break;

            case rapidjson::kStringType:
            {
                map->emplace(createKey(*segments),
                             std::string(value.GetString(), value.GetStringLength()));
            }
            break;

            case rapidjson::kNumberType:
            {
                map->emplace(createKey(*segments), base::numberToString(value.GetInt64()));
            }
            break;

            default:
            {
                LOG(LS_WARNING) << "Unexpected type: " << value.GetType();
            }
            break;
        }

        segments->pop_back();
    }
}

} // namespace

JsonSettings::JsonSettings(std::string_view file_name)
{
    path_ = filePath(file_name);
    if (path_.empty())
        return;

    readFile(path_, map());
}

JsonSettings::JsonSettings(Scope scope,
                           std::string_view application_name,
                           std::string_view file_name)
{
    path_ = filePath(scope, application_name, file_name);
    if (path_.empty())
        return;

    readFile(path_, map());
}

JsonSettings::~JsonSettings()
{
    if (isChanged())
        writeFile(path_, constMap());
}

bool JsonSettings::isWritable() const
{
    std::error_code error_code;

    if (std::filesystem::exists(path_, error_code))
    {
        std::ofstream stream;
        stream.open(path_, std::ofstream::binary);
        return stream.is_open();
    }
    else
    {
        if (!std::filesystem::create_directories(path_.parent_path(), error_code))
        {
            if (error_code)
                return false;
        }

        ScopedTempFile temp_file(path_);
        return temp_file.stream().is_open();
    }
}

void JsonSettings::sync()
{
    readFile(path_, map());
}

// static
std::filesystem::path JsonSettings::filePath(std::string_view file_name)
{
    if (file_name.empty())
        return std::filesystem::path();

    std::filesystem::path file_path;
    if (!BasePaths::currentExecDir(&file_path))
        return std::filesystem::path();

    file_path.append(file_name);
    file_path.replace_extension("json");

    return file_path;
}

// static
std::filesystem::path JsonSettings::filePath(Scope scope,
                                             std::string_view application_name,
                                             std::string_view file_name)
{
    if (application_name.empty() || file_name.empty())
        return std::filesystem::path();

    std::filesystem::path file_path;

    switch (scope)
    {
        case Scope::USER:
            BasePaths::userAppData(&file_path);
            break;

        case Scope::SYSTEM:
            BasePaths::commonAppData(&file_path);
            break;

        default:
            NOTREACHED();
            break;
    }

    if (file_path.empty())
        return std::filesystem::path();

    file_path.append(application_name);
    file_path.append(file_name);
    file_path.replace_extension("json");

    return file_path;
}

// static
bool JsonSettings::readFile(const std::filesystem::path& file, Map& map)
{
    map.clear();

    std::ifstream stream(file);
    rapidjson::IStreamWrapper wrapper(stream);

    rapidjson::Document doc;
    doc.ParseStream(wrapper);

    if (doc.HasParseError() || !doc.IsObject())
    {
        LOG(LS_ERROR) << "The configuration file is damaged: " << doc.GetParseError();
        return false;
    }

    std::vector<std::string_view> segments;
    parseObject(doc, &segments, &map);
    return true;
}

// static
bool JsonSettings::writeFile(const std::filesystem::path& file, const Map& map)
{
    std::error_code error_code;
    if (!std::filesystem::create_directories(file.parent_path(), error_code))
    {
        if (error_code)
            return false;
    }

    std::ofstream stream(file);
    rapidjson::OStreamWrapper wrapper(stream);

    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> json(wrapper);

    // Start JSON document.
    json.StartObject();

    std::vector<std::string_view> prev;

    for (const auto& map_item : map)
    {
        std::vector<std::string_view> segments =
            splitStringView(map_item.first, "/", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
        size_t count = 0;

        while (count < prev.size() && segments.at(count) == prev.at(count))
            ++count;

        for (int i = static_cast<int>(prev.size()) - 1; i > static_cast<int>(count); --i)
            json.EndObject();

        for (size_t i = count; i < segments.size(); ++i)
        {
            if (i != segments.size() - 1)
            {
                std::string_view& segment = segments.at(i);
                json.Key(segment.data(), segment.length());
                json.StartObject();
            }
        }

        std::string_view& segment = segments.back();
        json.Key(segment.data(), segment.length());
        json.String(map_item.second.data(), map_item.second.length());

        prev.swap(segments);
    }

    // End objects.
    for (size_t i = 0; i < prev.size() - 1; ++i)
        json.EndObject();

    // End JSON document.
    json.EndObject();

    return json.IsComplete() && !stream.fail();
}

} // namespace base
