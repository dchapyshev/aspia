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

#include "client/storage/user_settings.h"

#include <QLocale>

namespace {

const QString kLocaleParam = "locale";
const QString kThemeParam = "theme";
const QString kLastDatabaseParam = "last_database";

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(QSettings::IniFormat, QSettings::UserScope, "aspia", "client")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserSettings::~UserSettings() = default;

//--------------------------------------------------------------------------------------------------
QString UserSettings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::lastDatabase() const
{
    return settings_.value(kLastDatabaseParam).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLastDatabase(const QString& id)
{
    settings_.setValue(kLastDatabaseParam, id);
}
