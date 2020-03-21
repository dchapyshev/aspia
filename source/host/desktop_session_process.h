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

#ifndef HOST__DESKTOP_SESSION_PROCESS_H
#define HOST__DESKTOP_SESSION_PROCESS_H

#include "base/session_id.h"
#include "base/win/scoped_object.h"

#include <filesystem>
#include <memory>
#include <string_view>

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class DesktopSessionProcess
{
public:
    ~DesktopSessionProcess();

    static std::unique_ptr<DesktopSessionProcess> create(
        base::SessionId session_id, std::u16string_view channel_id);
    static std::filesystem::path filePath();

    void kill();

private:
    DesktopSessionProcess(base::win::ScopedHandle&& process, base::win::ScopedHandle&& thread);

    base::win::ScopedHandle process_;
    base::win::ScopedHandle thread_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionProcess);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_PROCESS_H
