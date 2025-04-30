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

#include "common/update_info.h"

#include "base/logging.h"
#include "base/strings/unicode.h"

#include "third_party/rapidxml/rapidxml.hpp"

namespace common {

//--------------------------------------------------------------------------------------------------
// static
UpdateInfo UpdateInfo::fromXml(const QByteArray& buffer)
{
    if (buffer.isEmpty())
    {
        LOG(LS_INFO) << "Empty XML buffer";
        return UpdateInfo();
    }

    LOG(LS_INFO) << "XML: " << QString::fromUtf8(buffer);

    if (!buffer.startsWith('<'))
    {
        // No updates or an error occurred on the update server.
        return UpdateInfo();
    }

    rapidxml::xml_document<> xml;

    try
    {
        xml.parse<rapidxml::parse_default>(const_cast<char*>(buffer.data()));
    }
    catch (const rapidxml::parse_error& error)
    {
        LOG(LS_ERROR) << "Invalid XML for update info: " << error.what() << " at "
                      << error.where<char>();
        return UpdateInfo();
    }

    rapidxml::xml_node<>* root_node = xml.first_node("update");
    if (!root_node)
    {
        LOG(LS_ERROR) << "Node 'update' not found. No available updates";
        return UpdateInfo();
    }

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
            {
                update_info.version_ = base::Version(base::utf16FromUtf8(
                    std::string_view(node->value(), node->value_size())));
                if (!update_info.version_.isValid())
                {
                    LOG(LS_ERROR) << "Invalid version: " << node->value();
                    return UpdateInfo();
                }
            }
            else if (name == "description")
            {
                static const size_t kMaxDescriptionLength = 2048;

                if (node->value_size() > kMaxDescriptionLength)
                {
                    LOG(LS_ERROR) << "Invalid description length: " << node->value_size()
                                  << " (max: " << kMaxDescriptionLength << ")";
                    return UpdateInfo();
                }

                update_info.description_ = QString::fromUtf8(
                    node->value(), static_cast<QString::size_type>(node->value_size()));
            }
            else if (name == "url")
            {
                static const size_t kMinUrlLength = 10;
                static const size_t kMaxUrlLength = 256;

                if (node->value_size() < kMinUrlLength || node->value_size() > kMaxUrlLength)
                {
                    LOG(LS_ERROR) << "Invalid URL length: " << node->value_size()
                                  << " (min: " << kMinUrlLength << " max: " << kMaxUrlLength << ")";
                    return UpdateInfo();
                }

                update_info.url_ = QString::fromUtf8(
                    node->value(), static_cast<QString::size_type>(node->value_size()));
            }
        }
    }

    update_info.valid_ = true;
    return update_info;
}

} // namespace common
