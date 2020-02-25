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

#include "base/xml_settings.h"

#include "base/logging.h"
#include "base/xml_sax_writer.h"
#include "base/files/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/strings/string_split.h"

#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

#include <fstream>

namespace base {

namespace {

bool parseXmlNode(const rapidxml::xml_node<>* node,
                  std::vector<std::string_view>* segments,
                  XmlSettings::Map* map)
{
    if (!node)
        return true;

    switch (node->type())
    {
        case rapidxml::node_element:
        {
            rapidxml::xml_attribute<>* name = node->first_attribute("Name");
            if (!name)
                return false;

            segments->emplace_back(name->value(), name->value_size());

            for (const rapidxml::xml_node<>* child_node = node->first_node();
                 child_node != nullptr;
                 child_node = child_node->next_sibling())
            {
                parseXmlNode(child_node, segments, map);
            }

            segments->pop_back();
        }
        break;

        case rapidxml::node_data:
        {
            std::string key;

            for (size_t i = 0; i < segments->size(); ++i)
            {
                if (i != 0)
                    key += Settings::kSeparator;

                key.append(segments->at(i));
            }

            if (key.empty())
                return false;

            map->emplace(std::move(key), std::string(node->value(), node->value_size()));
        }
        break;

        default:
            break;
    }

    return true;
}

} // namespace

XmlSettings::XmlSettings(Scope scope,
                         std::string_view application_name,
                         std::string_view file_name)
{
    path_ = filePath(scope, application_name, file_name);
    if (path_.empty())
        return;

    readFile(path_, map());
}

XmlSettings::~XmlSettings()
{
    if (isChanged())
        writeFile(path_, constMap());
}

bool XmlSettings::isWritable() const
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

void XmlSettings::sync()
{
    readFile(path_, map());
}

// static
std::filesystem::path XmlSettings::filePath(Scope scope,
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
    file_path.replace_extension("xml");

    return file_path;
}

// static
bool XmlSettings::readFile(const std::filesystem::path& file, Map& map)
{
    map.clear();

    std::string buffer;
    if (!base::readFile(file, &buffer))
        return false;

    rapidxml::xml_document<> doc;

    try
    {
        doc.parse<0>(buffer.data());
    }
    catch (const rapidxml::parse_error& error)
    {
        LOG(LS_ERROR) << "The configuration file is damaged: " << error.what();
        return false;
    }

    rapidxml::xml_node<>* root_node = doc.first_node("Settings");
    if (!root_node)
        return false;

    std::vector<std::string_view> segments;

    for (const rapidxml::xml_node<>* child_node = root_node->first_node();
         child_node != nullptr;
         child_node = child_node->next_sibling())
    {
        if (!parseXmlNode(child_node, &segments, &map))
            return false;
    }

    return true;
}

// static
bool XmlSettings::writeFile(const std::filesystem::path& file, const Map& map)
{
    std::error_code error_code;

    if (!std::filesystem::create_directories(file.parent_path(), error_code))
    {
        if (error_code)
            return false;
    }

    std::stringstream string_stream;

    XmlSaxWriter xml(string_stream);

    xml.startDocument();
    xml.startElement("Settings");

    std::vector<std::string_view> prev;

    for (const auto& map_item : map)
    {
        std::vector<std::string_view> segments =
            splitStringView(map_item.first, "/", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
        size_t count = 0;

        while (count < prev.size() && segments.at(count) == prev.at(count))
            ++count;

        for (size_t i = prev.size(); i > count; --i)
            xml.endElement();

        for (size_t i = count; i < segments.size(); ++i)
        {
            if (i == segments.size() - 1)
                xml.startElement("Value");
            else
                xml.startElement("Group");

            xml.attribute("Name", segments.at(i));
        }

        xml.text(map_item.second);
        prev.swap(segments);
    }

    for (size_t i = 0; i < prev.size(); ++i)
        xml.endElement();

    xml.endElement();
    xml.endDocument();

    std::ofstream file_stream;

    file_stream.open(file, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
    if (!file_stream.is_open())
        return false;

    // It is necessary to write a file in one operation.
    file_stream << string_stream.str();

    return !file_stream.fail();
}

} // namespace base
