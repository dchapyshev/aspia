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

#ifndef HOST_UI_USER_SETTINGS_H
#define HOST_UI_USER_SETTINGS_H

#include <QSettings>

namespace host {

class UserSettings
{
public:
    UserSettings();
    ~UserSettings();

    QString filePath() const;
    bool isWritable() const;
    void sync();

    QString locale() const;
    void setLocale(const QString& locale);

    bool showIconsInMenus() const;
    void setShowIconsInMenus(bool enable);

    quint32 oneTimeSessions() const;
    void setOneTimeSessions(quint32 sessions);

private:
    QSettings settings_;

    Q_DISABLE_COPY(UserSettings)
};

} // namespace host

#endif // HOST_UI_USER_SETTINGS_H
