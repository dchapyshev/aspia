//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_UPDATER__UPDATE_H_
#define ASPIA_UPDATER__UPDATE_H_

#include <string>

namespace aspia {

struct Update
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t build;
    std::string description;
    std::string url;
};

} // namespace aspia

#endif // ASPIA_UPDATER__UPDATE_H_
