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

#include "common/update_info.h"

#include "base/logging.h"

#include <rapidxml/rapidxml.hpp>

namespace common {

// static
UpdateInfo UpdateInfo::fromXml(const base::ByteArray& buffer)
{
    if (buffer.empty())
        return UpdateInfo();

    rapidxml::xml_document<> xml;

    try
    {
        xml.parse<rapidxml::parse_non_destructive>(
            reinterpret_cast<char*>(const_cast<uint8_t*>(buffer.data())));
    }
    catch (const rapidxml::parse_error& error)
    {
        LOG(LS_WARNING) << "Invalid XML for update info: " << error.what();
        return UpdateInfo();
    }

    rapidxml::xml_node<>* root_node = xml.first_node("update");
    if (!root_node)
        return UpdateInfo();

    UpdateInfo update_info;

    for (const rapidxml::xml_node<>* child_node = root_node->first_node();
         child_node != nullptr;
         child_node = child_node->next_sibling())
    {
        if (child_node->type() != rapidxml::node_element)
            continue;

        std::string_view name(child_node->name(), child_node->name_size());

        const rapidxml::xml_node<>* node = child_node->first_node();
        if (node && node->type() == rapidxml::node_data)
        {
            if (name == "version")
                update_info.version_ = base::Version(std::string_view(node->value(), node->value_size()));
            else if (name == "description")
                update_info.description_ = std::string(node->value(), node->value_size());
            else if (name == "url")
                update_info.url_ = std::string(node->value(), node->value_size());
        }
    }

    update_info.valid_ = true;
    return update_info;
}

bool UpdateInfo::hasUpdate() const
{
    return !version_.isValid() && !url_.empty();
}

} // namespace common
