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

#ifndef BASE_MAC_LOGIN_UTILS_H
#define BASE_MAC_LOGIN_UTILS_H

#include <QString>

#include "base/session_id.h"

class LoginUtils
{
public:
    // Identifies the macOS login-window session (no user logged in). It is captured by the launchd
    // LoginWindow agent (installAgent), not a per-user agent.
    static const SessionId kSessionId;

    // Returns true if the local console currently shows the macOS login window (no user logged in).
    static bool isActive();

    // Installs the launchd LaunchAgent that runs the desktop agent in the macOS login-window and Aqua
    // sessions. |app_path| is the aspia_host executable path. Requires root.
    static bool installAgent(const QString& app_path);

    // Removes the LaunchAgent installed by installAgent(). Requires root.
    static bool removeAgent();

private:
    Q_DISABLE_COPY_MOVE(LoginUtils)
};

#endif // BASE_MAC_LOGIN_UTILS_H
