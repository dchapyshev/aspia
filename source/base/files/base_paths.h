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

#ifndef BASE_FILES_BASE_PATHS_H
#define BASE_FILES_BASE_PATHS_H

#include <QString>

namespace base {

class BasePaths
{
public:
    // System-wide configuration directory, shared across all users and applications.
    //   Windows: C:\ProgramData
    //   Linux:   /etc
    //   MacOS:   /Library/Application Support
    static QString genericConfigDir();

    // Per-user configuration directory, shared across applications.
    //   Windows: C:\Users\<user>\AppData\Roaming
    //   Linux:   /home/<user>/.config
    //   MacOS:   /Users/<user>/Library/Application Support
    static QString genericUserConfigDir();

    // System-wide configuration directory dedicated to Aspia (genericConfigDir() + "/aspia").
    //   Windows: C:\ProgramData\aspia
    //   Linux:   /etc/aspia
    //   MacOS:   /Library/Application Support/aspia
    static QString appConfigDir();

    // Per-user configuration directory dedicated to Aspia (genericUserConfigDir() + "/aspia").
    //   Windows: C:\Users\<user>\AppData\Roaming\aspia
    //   Linux:   /home/<user>/.config/aspia
    //   MacOS:   /Users/<user>/Library/Application Support/aspia
    static QString appUserConfigDir();

    // System-wide data directory, shared across all users and applications.
    //   Windows: C:\ProgramData
    //   Linux:   /var/lib
    //   MacOS:   /Library/Application Support
    static QString genericDataDir();

    // Per-user data directory, shared across applications.
    //   Windows: C:\Users\<user>\AppData\Roaming
    //   Linux:   /home/<user>/.local/share
    //   MacOS:   /Users/<user>/Library/Application Support
    static QString genericUserDataDir();

    // System-wide data directory dedicated to Aspia (genericDataDir() + "/aspia").
    //   Windows: C:\ProgramData\aspia
    //   Linux:   /var/lib/aspia
    //   MacOS:   /Library/Application Support/aspia
    static QString appDataDir();

    // Per-user data directory dedicated to Aspia (genericUserDataDir() + "/aspia").
    //   Windows: C:\Users\<user>\AppData\Roaming\aspia
    //   Linux:   /home/<user>/.local/share/aspia
    //   MacOS:   /Users/<user>/Library/Application Support/aspia
    static QString appUserDataDir();

    // Home directory of the current user.
    //   Windows: C:\Users\<user>
    //   Linux:   /home/<user>
    //   MacOS:   /Users/<user>
    static QString userHome();

    // Full path to the executable file of the current process.
    static QString currentApp();

    // Directory containing the executable file of the current process.
    static QString currentAppDir();

private:
    Q_DISABLE_COPY_MOVE(BasePaths)
};

} // namespace base

#endif // BASE_FILES_BASE_PATHS_H
