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

#include "proto/peer.h"

#if defined(Q_OS_WINDOWS)
#include "host/win/portable_host.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

const QString kLocaleParam = "Locale";
const QString kThemeParam = "Theme";
const QString kOneTimeSessionsParam = "OneTimeSessions";
const QString kSecurityLogDialogStateParam = "SecurityLogDialogState";
const QString kSystemInfoWindowStateParam = "SystemInfoWindowState";

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(QSettings::IniFormat, QSettings::UserScope, "aspia", "host")
{
#if defined(Q_OS_WINDOWS)
    portable_ = PortableHost::isActive();
#endif // defined(Q_OS_WINDOWS)
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
    if (!portable_)
        settings_.sync();
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::locale() const
{
    return value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLocale(const QString& locale)
{
    setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::theme() const
{
    return value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setTheme(const QString& theme)
{
    setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
quint32 UserSettings::oneTimeSessions() const
{
    return value(kOneTimeSessionsParam, proto::peer::SESSION_TYPE_ALL).toUInt();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setOneTimeSessions(quint32 sessions)
{
    setValue(kOneTimeSessionsParam, sessions);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::securityLogDialogState() const
{
    return value(kSecurityLogDialogStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setSecurityLogDialogState(const QByteArray& state)
{
    setValue(kSecurityLogDialogStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray UserSettings::systemInfoWindowState() const
{
    return value(kSystemInfoWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setSystemInfoWindowState(const QByteArray& state)
{
    setValue(kSystemInfoWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QVariant UserSettings::value(const QString& key, const QVariant& default_value) const
{
    if (portable_)
        return default_value;

    return settings_.value(key, default_value);
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setValue(const QString& key, const QVariant& value)
{
    if (portable_)
        return;

    settings_.setValue(key, value);
}
