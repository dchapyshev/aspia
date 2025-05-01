//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/strings/string_split.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
std::string createKey(const std::vector<std::string_view>& segments)
{
    std::string key;

    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i != 0)
            key += Settings::kSeparator;
        key.append(segments[i]);
    }

    return key;
}

//--------------------------------------------------------------------------------------------------
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
                LOG(LS_ERROR) << "Unexpected type: " << value.GetType();
            }
            break;
        }

        segments->pop_back();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
JsonSettings::JsonSettings(std::string_view file_name)
{
    path_ = filePath(file_name);
    if (path_.empty())
        return;

    sync();
}

//--------------------------------------------------------------------------------------------------
JsonSettings::JsonSettings(Scope scope,
                           std::string_view application_name,
                           std::string_view file_name)
{
    path_ = filePath(scope, application_name, file_name);
    if (path_.empty())
        return;

    sync();
}

//--------------------------------------------------------------------------------------------------
JsonSettings::~JsonSettings()
{
    flush();
}

//--------------------------------------------------------------------------------------------------
bool JsonSettings::isWritable() const
{
    std::error_code error_code;

    if (std::filesystem::exists(path_, error_code))
    {
        std::ofstream stream;
        stream.open(path_, std::ofstream::binary | std::ofstream::app);
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

//--------------------------------------------------------------------------------------------------
void JsonSettings::sync()
{
    readFile(path_, map());
    setChanged(false);
}

//--------------------------------------------------------------------------------------------------
bool JsonSettings::flush()
{
    // If the configuration has no changes, then exit.
    if (!isChanged())
    {
        LOG(LS_INFO) << "No changes for: " << path_;
        return true;
    }

    // Write to the configuration file.
    if (writeFile(path_, map()))
    {
        LOG(LS_INFO) << "Configuration file '" << path_ << "' successfully written to disk";
        setChanged(false);
    }
    else
    {
        LOG(LS_ERROR) << "Unable to write configuration file: " << path_;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
// static
bool JsonSettings::readFile(const std::filesystem::path& file, Map& map)
{
    map.clear();

    std::error_code ignored_code;
    std::filesystem::file_status status = std::filesystem::status(file, ignored_code);

    if (!std::filesystem::exists(status))
    {
        // If the configuration file does not yet exist, then we write an empty file.
        writeFile(file, map);

        // The absence of a configuration file is normal case.
        return true;
    }

    if (!std::filesystem::is_regular_file(status))
    {
        LOG(LS_ERROR) << "Specified configuration file is not a file: '" << file << "'";
        return false;
    }

    if (std::filesystem::is_empty(file, ignored_code))
    {
        // The configuration file may be empty.
        return true;
    }

    static const uintmax_t kMaxFileSize = 5 * 1024 * 2024; // 5 Mb.

    uintmax_t file_size = std::filesystem::file_size(file, ignored_code);
    if (file_size > kMaxFileSize)
    {
        LOG(LS_ERROR) << "Too big settings file: '" << file << "' (" << file_size << " bytes)";
        return false;
    }

    std::string buffer;
    if (!base::readFile(file, &buffer))
    {
        LOG(LS_ERROR) << "Failed to read config file: '" << file << "'";
        return false;
    }

    rapidjson::StringStream stream(buffer.data());
    rapidjson::Document doc;
    doc.ParseStream(stream);

    if (doc.HasParseError())
    {
        LOG(LS_ERROR) << "The configuration file is damaged: '"
                      << file << "' (" << doc.GetParseError() << ")";
        return false;
    }

    std::vector<std::string_view> segments;
    parseObject(doc, &segments, &map);

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool JsonSettings::writeFile(const std::filesystem::path& file, const Map& map)
{
    std::error_code error_code;
    if (!std::filesystem::create_directories(file.parent_path(), error_code))
    {
        if (error_code)
        {
            LOG(LS_ERROR) << "create_directories failed: "
                          << base::utf16FromLocal8Bit(error_code.message());
            return false;
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> json(buffer);

    // Start JSON document.
    json.StartObject();

    std::vector<std::string_view> prev;

    for (const auto& map_item : map)
    {
        std::vector<std::string_view> segments =
            splitStringView(map_item.first, "/", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
        size_t count = 0;

        while (count < prev.size() && segments[count] == prev[count])
            ++count;

        for (int i = static_cast<int>(prev.size()) - 1; i > static_cast<int>(count); --i)
            json.EndObject();

        for (size_t i = count; i < segments.size(); ++i)
        {
            if (i != segments.size() - 1)
            {
                std::string_view& segment = segments[i];
                json.Key(segment.data(), static_cast<rapidjson::SizeType>(segment.length()));
                json.StartObject();
            }
        }

        std::string_view& segment = segments.back();
        json.Key(segment.data(), static_cast<rapidjson::SizeType>(segment.length()));
        json.String(map_item.second.data(), static_cast<rapidjson::SizeType>(map_item.second.length()));

        prev.swap(segments);
    }

    if (!prev.empty())
    {
        // End objects.
        for (size_t i = 0; i < prev.size() - 1; ++i)
            json.EndObject();
    }

    // End JSON document.
    json.EndObject();

    if (!json.IsComplete())
    {
        LOG(LS_ERROR) << "Incomplete json document";
        return false;
    }

    std::string_view source_buffer(buffer.GetString(), buffer.GetSize());

    if (!base::writeFile(file, source_buffer))
    {
        LOG(LS_ERROR) << "Failed to write config file";
        return false;
    }

    return true;
}

} // namespace base
