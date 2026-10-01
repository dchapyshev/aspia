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

#ifndef HOST_WIN_PORTABLE_HOST_H
#define HOST_WIN_PORTABLE_HOST_H

#include <QString>

class PortableHost
{
public:
    // Whether the executable of this process is the copy made by the launcher.
    static bool isStartedByLauncher();

    // Runs the launcher (asks for administrator rights, starts the temporary service, cleans up).
    static int runLauncher(int argc, char* argv[]);

    // Whether this process is the copy the launcher runs as the portable service.
    static bool isActive();

    // Name of the temporary service of the portable host this process belongs to.
    static QString serviceName();

    // IPC channel of this run's desktop agent (unique, for coexistence with the installed host).
    static QString desktopAgentChannelId();

    // IPC channel of this run's UI (unique, for coexistence with the installed host).
    static QString uiChannelId();

private:
    Q_DISABLE_COPY_MOVE(PortableHost)
};

#endif // HOST_WIN_PORTABLE_HOST_H
