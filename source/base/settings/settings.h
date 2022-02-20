//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_SETTINGS_SETTINGS_H
#define BASE_SETTINGS_SETTINGS_H

#include "base/converter.h"

#include <map>

namespace base {

namespace internal {

struct StringLess
{
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const
    {
        return lhs < rhs;
    }
};

} // namespace internal

class Settings
{
public:
    using Map = std::map<std::string, std::string, internal::StringLess>;
    using Array = std::vector<Settings>;

    static const std::string_view kSeparator;

    Settings() = default;
    virtual ~Settings() = default;

    Settings(const Settings& other);
    Settings& operator=(const Settings& other);

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
    T get(std::string_view key, const T& default_value = defaultValue<T>()) const
    {
        Map::const_iterator result = map_.find(key);
        if (result == map_.cend())
            return default_value;

        return Converter<T>::get_value(result->second).value_or(default_value);
    }

    template <typename T>
    void set(std::string_view key, const T& value)
    {
        map_.insert_or_assign(std::string(key), Converter<T>::set_value(value));
        is_changed_ = true;
    }

    Array getArray(std::string_view key) const;
    void setArray(std::string_view key, const Array& array);

    Settings getGroup(std::string_view key) const;
    void setGroup(std::string_view key, const Settings& group);

    void remove(std::string_view key);

    const Map& constMap() const { return map_; }
    Map& map() { return map_; }

    bool isChanged() const { return is_changed_; }

private:
    bool is_changed_ = false;
    Map map_;
};

} // namespace base

#endif // BASE_SETTINGS_SETTINGS_H
