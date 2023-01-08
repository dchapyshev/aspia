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

#ifndef BASE_STL_UTIL_H
#define BASE_STL_UTIL_H

#include <algorithm>
#include <deque>
#include <forward_list>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace base {

namespace internal {

// Utility type traits used for specializing base::contains() below.
template <typename Container, typename Element, typename = void>
struct HasFindWithNpos : std::false_type
{
    // Nothing
};

template <typename Container, typename Element>
struct HasFindWithNpos<Container,
                       Element,
                       std::void_t<decltype(std::declval<const Container&>().find(
                           std::declval<const Element&>()) != Container::npos)>>
    : std::true_type
{
    // Nothing
};

template <typename Container, typename Element, typename = void>
struct HasFindWithEnd : std::false_type
{
    // Nothing
};

template <typename Container, typename Element>
struct HasFindWithEnd<Container,
                      Element,
                      std::void_t<decltype(std::declval<const Container&>().find(
                          std::declval<const Element&>()) != std::declval<const Container&>().end())>>
    : std::true_type
{
    // Nothing
};

template <typename Container, typename Element, typename = void>
struct HasContains : std::false_type
{
    // Nothing
};

template <typename Container, typename Element>
struct HasContains<Container,
                   Element,
                   std::void_t<decltype(std::declval<const Container&>().contains(
                       std::declval<const Element&>()))>> : std::true_type
{
    // Nothing
};

} // namespace internal

// General purpose implementation to check if |container| contains |value|.
template <typename Container,
          typename Value,
          std::enable_if_t<
              !internal::HasFindWithNpos<Container, Value>::value &&
              !internal::HasFindWithEnd<Container, Value>::value &&
              !internal::HasContains<Container, Value>::value>* = nullptr>
bool contains(const Container& container, const Value& value)
{
    return std::find(std::begin(container), std::end(container), value) != std::end(container);
}

// Specialized contains() implementation for when |container| has a find() member function and a
// static npos member, but no contains() member function.
template <typename Container,
          typename Value,
          std::enable_if_t<internal::HasFindWithNpos<Container, Value>::value &&
                           !internal::HasContains<Container, Value>::value>* = nullptr>
bool contains(const Container& container, const Value& value)
{
    return container.find(value) != Container::npos;
}

// Specialized contains() implementation for when |container| has a find() and end() member function,
// but no contains() member function.
template <typename Container,
          typename Value,
          std::enable_if_t<internal::HasFindWithEnd<Container, Value>::value &&
                           !internal::HasContains<Container, Value>::value>* = nullptr>
bool contains(const Container& container, const Value& value)
{
    return container.find(value) != container.end();
}

// Specialized contains() implementation for when |container| has a contains() member function.
template <
    typename Container,
    typename Value,
    std::enable_if_t<internal::HasContains<Container, Value>::value>* = nullptr>
bool contains(const Container& container, const Value& value)
{
    return container.contains(value);
}


} // namespace base

#endif // BASE_STL_UTIL_H
