//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__HOST_KEY_STORAGE_H
#define HOST__HOST_KEY_STORAGE_H

#include "base/macros_magic.h"
#include "base/settings/json_settings.h"

namespace host {

class HostKeyStorage
{
public:
    HostKeyStorage();
    ~HostKeyStorage();

    base::ByteArray key(std::string_view session_name) const;
    void setKey(std::string_view session_name, const base::ByteArray& key);

private:
    base::JsonSettings impl_;

    DISALLOW_COPY_AND_ASSIGN(HostKeyStorage);
};

} // namespace host

#endif // HOST__HOST_KEY_STORAGE_H
