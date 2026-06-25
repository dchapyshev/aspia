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

#ifndef BASE_LINUX_SCOPED_USER_CREDENTIALS_H
#define BASE_LINUX_SCOPED_USER_CREDENTIALS_H

#include <QtClassHelperMacros>

#include <sys/types.h>

// Temporarily assumes the credentials of a session user (real and effective uid/gid; the saved ids
// stay 0, so root is restored on scope exit). This lets the root host process pass the per-user
// authentication on the session bus and the session's PipeWire daemon, both of which check the
// connecting peer's effective uid (SO_PEERCRED). The credentials only need to be held for the socket
// connect, which captures the peer credentials; the connection then stays authenticated as that user.
class ScopedUserCredentials
{
public:
    explicit ScopedUserCredentials(uid_t uid);
    ~ScopedUserCredentials();

    // Returns true if the credentials were assumed (false leaves the process unchanged - as root).
    bool isActive() const { return active_; }

private:
    bool active_ = false;

    Q_DISABLE_COPY_MOVE(ScopedUserCredentials)
};

#endif // BASE_LINUX_SCOPED_USER_CREDENTIALS_H
