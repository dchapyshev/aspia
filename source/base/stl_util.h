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

#ifndef BASE__STL_UTIL_H
#define BASE__STL_UTIL_H

#include <algorithm>
#include <type_traits>

namespace base {

namespace internal {

template <typename Collection>
class HasKeyType
{
    template <typename C>
    static std::true_type test(typename C::key_type*);
    template <typename C>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<Collection>(nullptr))::value;
};

} // namespace internal

template <typename Collection,
          typename Value,
          typename std::enable_if<!internal::HasKeyType<Collection>::value, int>::type = 0>
bool contains(const Collection& collection, const Value& value)
{
    return std::find(std::begin(collection), std::end(collection), value) !=
                     std::end(collection);
}

// Test to see if a set or map contains a particular key.
// Returns true if the key is in the collection.
template <typename Collection,
          typename Key,
          typename std::enable_if<internal::HasKeyType<Collection>::value, int>::type = 0>
bool contains(const Collection& collection, const Key& key)
{
    return collection.find(key) != collection.end();
}

} // namespace base

#endif // BASE__STL_UTIL_H
