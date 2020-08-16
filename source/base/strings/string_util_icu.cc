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

#include "base/strings/string_util.h"

#include "base/logging.h"

namespace base {

int compareCaseInsensitive(std::u16string_view /* first */, std::u16string_view /* second */)
{
    NOTIMPLEMENTED();
    return 0;
}

std::u16string toUpper(std::u16string_view in)
{
    NOTIMPLEMENTED();
    return std::u16string();
}

std::u16string toLower(std::u16string_view in)
{
    NOTIMPLEMENTED();
    return std::u16string();
}

} // namespace base
