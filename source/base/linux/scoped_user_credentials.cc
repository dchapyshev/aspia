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

#include "base/linux/scoped_user_credentials.h"

#include <pwd.h>
#include <unistd.h>

#include <vector>

#include "base/logging.h"

namespace {

//--------------------------------------------------------------------------------------------------
gid_t primaryGid(uid_t uid)
{
    long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buffer_size < 0)
        buffer_size = 16384;

    std::vector<char> buffer(static_cast<size_t>(buffer_size));
    passwd pw;
    passwd* result = nullptr;

    if (getpwuid_r(uid, &pw, buffer.data(), buffer.size(), &result) == 0 && result)
        return pw.pw_gid;

    return uid;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScopedUserCredentials::ScopedUserCredentials(uid_t uid)
{
    const gid_t gid = primaryGid(uid);

    // The gid must be set before the uid: dropping the uid first would remove the privilege needed to
    // change the gid.
    if (setresgid(gid, gid, 0) != 0)
    {
        PLOG(ERROR) << "setresgid failed";
        return;
    }

    if (setresuid(uid, uid, 0) != 0)
    {
        PLOG(ERROR) << "setresuid failed";
        if (setresgid(0, 0, 0) != 0)
            PLOG(ERROR) << "setresgid restore failed";
        return;
    }

    active_ = true;
}

//--------------------------------------------------------------------------------------------------
ScopedUserCredentials::~ScopedUserCredentials()
{
    if (!active_)
        return;

    // The saved ids are still 0, so root can be restored. Failing to do so leaves the whole process
    // unprivileged, which it cannot recover from.
    if (setresuid(0, 0, 0) != 0)
        PLOG(FATAL) << "Unable to restore root uid";
    if (setresgid(0, 0, 0) != 0)
        PLOG(FATAL) << "Unable to restore root gid";
}
