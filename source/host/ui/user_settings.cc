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

#include "host/ui/user_settings.h"

#include "qt_base/qt_xml_settings.h"

#include <QLocale>

namespace host {

UserSettings::UserSettings()
    : settings_(qt_base::QtXmlSettings::format(),
                QSettings::UserScope,
                QLatin1String("aspia"),
                QLatin1String("host"))
{
    // Nothing
}

UserSettings::~UserSettings() = default;

QString UserSettings::filePath() const
{
    return settings_.fileName();
}

bool UserSettings::isWritable() const
{
    return settings_.isWritable();
}

void UserSettings::sync()
{
    settings_.sync();
}

QString UserSettings::locale() const
{
    return settings_.value(QLatin1String("Locale"), QLocale::system().bcp47Name()).toString();
}

void UserSettings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

} // namespace host
