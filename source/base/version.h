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

#ifndef BASE__VERSION_H
#define BASE__VERSION_H

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace base {

// Version represents a dotted version number, like "1.2.3.4", supporting parsing and comparison.
class Version
{
public:
    // The only thing you can legally do to a default constructed Version object is assign to it.
    Version();

    Version(const Version& other);

    // Initializes from a decimal dotted version number, like "0.1.1".
    // Each component is limited to a uint16_t. Call isValid() to learn the outcome.
    explicit Version(const std::string& version_str);

    // Initializes from a vector of components, like {1, 2, 3, 4}. Call isValid() to learn the
    // outcome.
    explicit Version(const std::vector<uint32_t>& components);

    ~Version();

    // Returns true if the object contains a valid version number.
    bool isValid() const;

    // Returns true if the version wildcard string is valid. The version wildcard string may end
    // with ".*" (e.g. 1.2.*, 1.*). Any other arrangement with "*" is invalid (e.g. 1.*.3 or 1.2.3*).
    // This functions defaults to standard Version behavior (isValid) if no wildcard is present.
    static bool isValidWildcardString(const std::string& wildcard_string);

    // Returns -1, 0, 1 for <, ==, >.
    int compareTo(const Version& other) const;

    // Given a valid version object, compare if a |wildcard_string| results in a newer version.
    // This function will default to CompareTo if the string does not end in wildcard sequence
    // ".*". isValidWildcard(wildcard_string) must be true before using this function.
    int compareToWildcardString(const std::string& wildcard_string) const;

    // Return the string representation of this version.
    const std::string asString() const;

    const std::vector<uint32_t>& components() const { return components_; }

private:
    std::vector<uint32_t> components_;
};

bool operator==(const Version& v1, const Version& v2);
bool operator!=(const Version& v1, const Version& v2);
bool operator<(const Version& v1, const Version& v2);
bool operator<=(const Version& v1, const Version& v2);
bool operator>(const Version& v1, const Version& v2);
bool operator>=(const Version& v1, const Version& v2);
std::ostream& operator<<(std::ostream& stream, const Version& v);

} // namespace base

#endif // BASE__VERSION_H
