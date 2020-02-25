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

#include "base/settings.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace base {

Settings::Settings(const Map& map)
    : map_(map)
{
    // Nothing
}

Settings::Settings(Map&& map) noexcept
    : map_(std::move(map))
{
    // Nothing
}

Settings::Array Settings::getArray(const std::string& key) const
{
    const size_t array_size = get<size_t>(key + kSeparator + "size");
    Array result;

    for (size_t i = 0; i < array_size; ++i)
        result.emplace_back(getGroup(key + kSeparator + base::numberToString(i)));

    return result;
}

void Settings::setArray(const std::string& key, const Array& array)
{
    for (size_t i = 0; i < array.size(); ++i)
        setGroup(key + kSeparator + base::numberToString(i), array[i]);

    set(key + kSeparator + "size", array.size());

    is_changed_ = true;
}

Settings Settings::getGroup(const std::string& key) const
{
    const std::string prefix = key + kSeparator;
    Map map;

    for (auto it = map_.cbegin(); it != map_.cend(); ++it)
    {
        if (base::startsWith(it->first, prefix))
            map.insert_or_assign(it->first.substr(prefix.length()), it->second);
    }

    return Settings(std::move(map));
}

void Settings::setGroup(const std::string& key, const Settings& group)
{
    const Map& array_map = group.constMap();

    for (auto it = array_map.cbegin(); it != array_map.cend(); ++it)
        map_.insert_or_assign(key + kSeparator + it->first, it->second);

    is_changed_ = true;
}

void Settings::remove(const std::string& key)
{
    for (auto it = map_.begin(); it != map_.end();)
    {
        if (base::startsWith(it->first, key + kSeparator))
            it = map_.erase(it);
        else
            ++it;
    }

    is_changed_ = true;
}

} // namespace base
