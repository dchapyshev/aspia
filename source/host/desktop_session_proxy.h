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

#ifndef HOST_DESKTOP_SESSION_PROXY_H
#define HOST_DESKTOP_SESSION_PROXY_H

#include "base/macros_magic.h"
#include "base/memory/local_memory.h"
#include "host/desktop_session.h"

namespace host {

class DesktopSessionManager;

class DesktopSessionProxy final : public base::enable_shared_from_this<DesktopSessionProxy>
{
public:
    DesktopSessionProxy();
    ~DesktopSessionProxy();

private:
    friend class DesktopSessionManager;

    //void attachAndStart(DesktopSession* desktop_session);
    //void stopAndDettach();

    DesktopSession* desktop_session_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_PROXY_H
