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

#ifndef BASE_SYSTEM_TIME_H
#define BASE_SYSTEM_TIME_H

namespace base {

class SystemTime
{
public:
    ~SystemTime() = default;

    SystemTime(const SystemTime& other) = default;
    SystemTime& operator=(const SystemTime& other) = default;

    // Returns current system time.
    static SystemTime now();

    // Four digit year "2019".
    int year() const { return data_.year; }

    // 1-based month (values 1 = January, etc.).
    int month() const { return data_.month; }

    // 1-based day of month (1-31).
    int day() const { return data_.day; }

    // Hour within the current day (0-23).
    int hour() const { return data_.hour; }

    // Minute within the current hour (0-59).
    int minute() const { return data_.minute; }

    // Second within the current minute (0-59 plus leap seconds which may take it up to 60).
    int second() const { return data_.second; }

    // // Milliseconds within the current second (0-999).
    int millisecond() const { return data_.millisecond; }

private:
    struct Data
    {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        int millisecond;
    };

    explicit SystemTime(const Data& data);

    Data data_;
};

} // namespace base

#endif // BASE_SYSTEM_TIME_H
