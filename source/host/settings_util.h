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

#ifndef HOST_SETTINGS_UTIL_H
#define HOST_SETTINGS_UTIL_H

#include <QCoreApplication>

class QWidget;

class SettingsUtil
{
    Q_DECLARE_TR_FUNCTIONS(SettingsUtil)

public:
    static bool importFromFile(const QString& path, bool silent, QWidget* parent = nullptr);
    static bool exportToFile(const QString& path, bool silent, QWidget* parent = nullptr);

private:
    // Platform-specific dialogs: MsgBox on the desktop, MessageDialog on Android.
    static bool confirmOverwrite(QWidget* parent);
    static void showError(QWidget* parent, const QString& text);
    static void showInfo(QWidget* parent, const QString& text);

    Q_DISABLE_COPY_MOVE(SettingsUtil)
};

#endif // HOST_SETTINGS_UTIL_H
