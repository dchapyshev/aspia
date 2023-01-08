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

#include "base/strings/string_util.h"

#include "base/logging.h"

#include <unicode/stringoptions.h>
#include <unicode/utypes.h>
#include <unicode/ustring.h>

namespace base {

int compareCaseInsensitive(std::u16string_view first, std::u16string_view second)
{
    UErrorCode error_code = U_ZERO_ERROR;
    int ret = u_strCaseCompare(first.data(), first.length(),
                               second.data(), second.length(),
                               U_FOLD_CASE_DEFAULT,
                               &error_code);
    CHECK(U_SUCCESS(error_code));

    return ret;
}

std::u16string toUpper(std::u16string_view in)
{
    if (in.empty())
        return std::u16string();

    UErrorCode error_code = U_ZERO_ERROR;
    int32_t len = u_strToUpper(nullptr, 0, in.data(), in.length(), "", &error_code);
    if (error_code != U_BUFFER_OVERFLOW_ERROR || len <= 0)
        return std::u16string();

    error_code = U_ZERO_ERROR;

    std::u16string out;
    out.resize(len);

    len = u_strToUpper(out.data(), out.length(), in.data(), in.length(), "", &error_code);
    if (!U_SUCCESS(error_code) || len <= 0)
        return std::u16string();

    return out;
}

std::u16string toLower(std::u16string_view in)
{
    if (in.empty())
        return std::u16string();

    UErrorCode error_code = U_ZERO_ERROR;
    int32_t len = u_strToLower(nullptr, 0, in.data(), in.length(), nullptr, &error_code);
    if (error_code != U_BUFFER_OVERFLOW_ERROR || len <= 0)
        return std::u16string();

    error_code = U_ZERO_ERROR;

    std::u16string out;
    out.resize(len);

    len = u_strToLower(out.data(), out.length(), in.data(), in.length(), nullptr, &error_code);
    if (!U_SUCCESS(error_code) || len <= 0)
        return std::u16string();

    return out;
}

} // namespace base
