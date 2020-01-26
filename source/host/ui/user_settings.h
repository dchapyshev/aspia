//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__UI__USER_SETTINGS_H
#define HOST__UI__USER_SETTINGS_H

#include "base/macros_magic.h"

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

private:
    QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(UserSettings);
};

} // namespace host

#endif // HOST__UI__USER_SETTINGS_H
