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

#include "host/ui/user_settings.h"

#include <QLocale>

#include "base/xml_settings.h"
#include "proto/peer.h"

namespace host {

namespace {

const QString kLocaleParam = QStringLiteral("Locale");
const QString kShowIconsInMenusParam = QStringLiteral("ShowIconsInMenus");
const QString kOneTimeSessionsParam = QStringLiteral("OneTimeSessions");

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(base::XmlSettings::format(), QSettings::UserScope, "aspia", "host")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserSettings::~UserSettings() = default;

//--------------------------------------------------------------------------------------------------
QString UserSettings::filePath() const
{
    return settings_.fileName();
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isWritable() const
{
    return settings_.isWritable();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::sync()
{
    settings_.sync();
}

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
bool UserSettings::showIconsInMenus() const
{
    bool defaultValue;

#if defined(Q_OS_MACOS)
    defaultValue = false;
#else
    defaultValue = true;
#endif

    return settings_.value(kShowIconsInMenusParam, defaultValue).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setShowIconsInMenus(bool enable)
{
    settings_.setValue(kShowIconsInMenusParam, enable);
}

//--------------------------------------------------------------------------------------------------
quint32 UserSettings::oneTimeSessions() const
{
    return settings_.value(kOneTimeSessionsParam, proto::peer::SESSION_TYPE_ALL).toUInt();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOneTimeSessions(quint32 sessions)
{
    settings_.setValue(kOneTimeSessionsParam, sessions);
}

} // namespace host
