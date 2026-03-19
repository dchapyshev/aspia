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

#ifndef HOST_HOST_UTILS_H
#define HOST_HOST_UTILS_H

#include <QtGlobal>

namespace host {

class HostUtils
{
public:
    static bool isMigrationNeeded();
    static void doMigrate();

    // Performs integrity checks on host components.
    // Returns true if check succeeded. Returns false if the check fails.
    static bool integrityCheck();

    enum DebugInfoFeatures
    {
        INCLUDE_DEFAULT = 0,
        INCLUDE_VIDEO_ADAPTERS = 1,
        INCLUDE_WINDOW_STATIONS = 2
    };

    static void printDebugInfo(quint32 features = INCLUDE_DEFAULT);
    static void uninstallApplication();

private:
    Q_DISABLE_COPY_MOVE(HostUtils)
};

} // namespace host

#endif // HOST_HOST_UTILS_H
