//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

void Settings::remove(std::string_view key)
{
    if (map_.empty())
        return;

    std::string prefix(key);
    prefix += kSeparator;

    for (auto it = map_.begin(); it != map_.end();)
    {
        if (base::startsWith(it->first, prefix))
            it = map_.erase(it);
        else
            ++it;
    }
}

size_t Settings::beginReadArray(std::string_view name)
{
    array_prefix_ = name;
    array_prefix_ += kSeparator;
    array_index_ = 0;
    array_size_ = 0;

    auto result = map_.find(array_prefix_ + "size");
    if (result == map_.cend())
        return 0;

    std::optional<size_t> size = StreamConverter<size_t>::get_value(result->second);
    if (!size.has_value())
        return 0;

    return size.value();
}

void Settings::beginWriteArray(std::string_view name)
{
    array_prefix_ = name;
    array_prefix_ += kSeparator;
}

void Settings::setArrayIndex(size_t index)
{
    array_index_ = index;
    ++array_size_;
}

void Settings::endArray()
{
    map_.insert_or_assign(array_prefix_ + "size",
        StreamConverter<size_t>::set_value(array_size_ + 1).value_or(std::string()));

    array_prefix_.clear();
}

std::string Settings::actualKeyName(const std::string& key) const
{
    if (array_prefix_.empty())
        return key;

    return array_prefix_ + std::to_string(array_index_) + kSeparator + key;
}

} // namespace base
