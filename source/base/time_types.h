//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_TIME_TYPES_H
#define BASE_TIME_TYPES_H

#include <chrono>

// The aliases live in a named namespace pulled in with a using-directive instead of being declared
// directly in the global namespace: on macOS the Carbon headers (included by Cocoa) already define
// global |Nanoseconds| and |Microseconds|, and a using-directive coexists with them as long as a
// translation unit does not reference the clashing name unqualified.
namespace time_types {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;
using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;

template <typename To, typename Rep, typename Period>
constexpr To DurationCast(const std::chrono::duration<Rep, Period>& duration)
{
    return std::chrono::duration_cast<To>(duration);
}

} // namespace time_types

using namespace time_types;

#endif // BASE_TIME_TYPES_H
