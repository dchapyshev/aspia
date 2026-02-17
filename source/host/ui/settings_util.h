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

#ifndef HOST_UI_SETTINGS_UTIL_H
#define HOST_UI_SETTINGS_UTIL_H

#include <QCoreApplication>

namespace host {

class SettingsUtil
{
    Q_DECLARE_TR_FUNCTIONS(SettingsUtil)

public:
    static bool importFromFile(const QString& path, bool silent, QWidget* parent = nullptr);
    static bool exportToFile(const QString& path, bool silent, QWidget* parent = nullptr);

private:
    static bool copySettings(const QString& source_path,
                             const QString& target_path,
                             bool silent,
                             QWidget* parent);

    Q_DISABLE_COPY(SettingsUtil)
};

} // namespace host

#endif // HOST_UI_SETTINGS_UTIL_H
