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
    static const char kSeparator = '/';

    Settings() = default;
    virtual ~Settings() = default;

    Settings(const Settings& other) = default;
    Settings& operator=(const Settings& other) = default;

    Settings(Settings&& other) = default;
    Settings& operator=(Settings&& other) = default;

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
        Map::const_iterator result = map_.find(actualKeyName(key));
        if (result == map_.cend())
            return default_value;

        return StreamConverter<T>::get_value(result->second).value_or(default_value);
    }

    template <typename T>
    void set(const std::string& key, const T& value)
    {
        map_.insert_or_assign(actualKeyName(key),
            StreamConverter<T>::set_value(value).value_or(std::string()));
    }

    void remove(std::string_view key);

    size_t beginReadArray(std::string_view name);
    void beginWriteArray(std::string_view name);
    void setArrayIndex(size_t index);
    void endArray();

    const Map& constMap() const { return map_; }
    Map& map() { return map_; }

private:
    std::string actualKeyName(const std::string& key) const;

    Map map_;

    std::string array_prefix_;
    size_t array_index_ = 0;
    size_t array_size_ = 0;
};

} // namespace base

#endif // BASE__SETTINGS_H
