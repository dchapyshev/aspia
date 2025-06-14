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

#ifndef BASE_LOCATION_H
#define BASE_LOCATION_H

#include <QDebug>
#include <QMetaType>
#include <QString>

namespace base {

// Location provides basic info where of an object was constructed, or was significantly brought
// to life.
class Location
{
public:
    Location();

    Location(const Location& other);
    Location& operator=(const Location& other);

    // Constructor should be called with a long-lived char*, such as __FILE__. It assumes the
    // provided value will persist as a global constant, and it will not make a copy of it.
    Location(const char* function_name, const char* file_name, int line_number);

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* functionName() const { return function_name_; }

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* fileName() const { return file_name_; }

    // Will be -1 for default initialized Location objects and when source names are disabled.
    int lineNumber() const { return line_number_; }

    // Converts to the most user-readable form possible.
    QString toString() const;

    static Location createFromHere(const char* function_name, const char* file_name, int line_number);

private:
    const char* function_name_ = nullptr;
    const char* file_name_ = nullptr;
    int line_number_ = -1;
};

// Full source information should be included.
#define FROM_HERE FROM_HERE_WITH_EXPLICIT_FUNCTION(__func__)
#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name) \
    ::base::Location::createFromHere(function_name, __FILE__, __LINE__)

} // namespace base

Q_DECLARE_METATYPE(base::Location)

QDebug operator<<(QDebug out, const base::Location& location);

#endif // BASE_LOCATION_H
