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

#include "router/workspace.h"

#include "base/string_util.h"

//--------------------------------------------------------------------------------------------------
// static
bool Workspace::isValidName(std::string_view name)
{
    const std::string_view trimmed = strTrimmed(name);
    return !trimmed.empty() && trimmed.size() <= kMaxNameLength;
}

//--------------------------------------------------------------------------------------------------
bool Workspace::isValid() const
{
    return entry_id > 0 && isValidName(name);
}
