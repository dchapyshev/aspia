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

#include "base/settings/settings.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace base {

const std::string_view Settings::kSeparator = { "/" };

//--------------------------------------------------------------------------------------------------
Settings::Settings(const Settings& other)
    : map_(other.map_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings& Settings::operator=(const Settings& other)
{
    if (&other != this)
        map_ = other.map_;

    return *this;
}

//--------------------------------------------------------------------------------------------------
Settings::Settings(const Map& map)
    : map_(map)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::Settings(Map&& map) noexcept
    : map_(std::move(map))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::Array Settings::getArray(std::string_view key) const
{
    const size_t array_size = get<size_t>(strCat({ key, kSeparator, "size" }));
    Array result;

    for (size_t i = 0; i < array_size; ++i)
    {
        result.emplace_back(getGroup(
            strCat({ key, kSeparator, numberToString(i + 1) })));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void Settings::setArray(std::string_view key, const Array& array)
{
    for (size_t i = 0; i < array.size(); ++i)
        setGroup(strCat({ key, kSeparator, numberToString(i + 1) }), array[i]);

    set(strCat({ key, kSeparator, "size" }), array.size());
}

//--------------------------------------------------------------------------------------------------
Settings Settings::getGroup(std::string_view key) const
{
    const std::string prefix = strCat({ key, kSeparator });
    Map map;

    for (auto it = map_.cbegin(); it != map_.cend(); ++it)
    {
        if (it->first.starts_with(prefix))
            map.insert_or_assign(it->first.substr(prefix.length()), it->second);
    }

    return Settings(std::move(map));
}

//--------------------------------------------------------------------------------------------------
void Settings::setGroup(std::string_view key, const Settings& group)
{
    const Map& array_map = group.constMap();

    for (auto it = array_map.cbegin(); it != array_map.cend(); ++it)
        map_.insert_or_assign(strCat({ key, kSeparator, it->first }), it->second);

    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void Settings::remove(std::string_view key)
{
    const std::string prefix = strCat({ key, kSeparator });

    for (auto it = map_.begin(); it != map_.end();)
    {
        if (it->first.starts_with(prefix))
            it = map_.erase(it);
        else
            ++it;
    }

    setChanged(true);
}

} // namespace base
