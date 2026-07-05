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

#include "host/user_settings.h"

#include <QLocale>

#include "base/xml_settings.h"
#include "proto/peer.h"

namespace {

const QString kLocaleParam = "Locale";
const QString kThemeParam = "Theme";
const QString kOneTimeSessionsParam = "OneTimeSessions";
const QString kSecurityLogDialogStateParam = "SecurityLogDialogState";

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(XmlSettings::format(), QSettings::UserScope, "aspia", "host")
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
quint32 UserSettings::oneTimeSessions() const
{
    return settings_.value(kOneTimeSessionsParam, proto::peer::SESSION_TYPE_ALL).toUInt();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOneTimeSessions(quint32 sessions)
{
    settings_.setValue(kOneTimeSessionsParam, sessions);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::securityLogDialogState() const
{
    return settings_.value(kSecurityLogDialogStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setSecurityLogDialogState(const QByteArray& state)
{
    settings_.setValue(kSecurityLogDialogStateParam, state);
}
