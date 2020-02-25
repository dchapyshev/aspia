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

#ifndef BASE__SETTINGS_H
#define BASE__SETTINGS_H

#include "base/stream_converter.h"

#include <map>
#include <vector>

namespace base {

class Settings
{
public:
    using Map = std::map<std::string, std::string>;
    using Array = std::vector<Settings>;

    static const char kSeparator = '/';

    Settings() = default;
    virtual ~Settings() = default;

    Settings(const Settings& other) = default;
    Settings& operator=(const Settings& other) = default;

    Settings(Settings&& other) noexcept = default;
    Settings& operator=(Settings&& other) noexcept = default;

    explicit Settings(const Map& map);
    explicit Settings(Map&& map) noexcept;

    template <typename T, typename std::enable_if<!std::is_arithmetic<T>::value>::type* = nullptr>
    static T defaultValue()
    {
        return T();
    }

    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
    static T defaultValue()
    {
        // For all arithmetic types, return 0 as the default value.
        return static_cast<T>(0);
    }

    template <typename T>
    T get(const std::string& key, const T& default_value = defaultValue<T>()) const
    {
        Map::const_iterator result = map_.find(key);
        if (result == map_.cend())
            return default_value;

        return StreamConverter<T>::get_value(result->second).value_or(default_value);
    }

    template <typename T>
    void set(const std::string& key, const T& value)
    {
        map_.insert_or_assign(key, StreamConverter<T>::set_value(value).value_or(std::string()));
        is_changed_ = true;
    }

    Array getArray(const std::string& key) const;
    void setArray(const std::string& key, const Array& array);

    Settings getGroup(const std::string& key) const;
    void setGroup(const std::string& key, const Settings& group);

    void remove(const std::string& key);

    const Map& constMap() const { return map_; }
    Map& map() { return map_; }

    bool isChanged() const { return is_changed_; }

private:
    bool is_changed_ = false;
    Map map_;
};

} // namespace base

#endif // BASE__SETTINGS_H
