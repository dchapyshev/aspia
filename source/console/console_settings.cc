//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/console_settings.h"

#include <QDir>

namespace aspia {

ConsoleSettings::ConsoleSettings()
    : settings_(QSettings::UserScope, QStringLiteral("Aspia"), QStringLiteral("Console"))
{
    // Nothing
}

// static
QString ConsoleSettings::defaultLocale()
{
    return QStringLiteral("en");
}

QString ConsoleSettings::locale() const
{
    return settings_.value(QStringLiteral("Locale"), defaultLocale()).toString();
}

void ConsoleSettings::setLocale(const QString& locale)
{
    settings_.setValue(QStringLiteral("Locale"), locale);
}

QString ConsoleSettings::lastDirectory() const
{
    return settings_.value(QStringLiteral("LastDirectory"), QDir::homePath()).toString();
}

void ConsoleSettings::setLastDirectory(const QString& directory_path)
{
    settings_.setValue(QStringLiteral("LastDirectory"), directory_path);
}

QByteArray ConsoleSettings::windowGeometry() const
{
    return settings_.value(QStringLiteral("WindowGeometry")).toByteArray();
}

void ConsoleSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("WindowGeometry"), geometry);
}

QByteArray ConsoleSettings::windowState() const
{
    return settings_.value(QStringLiteral("WindowState")).toByteArray();
}

void ConsoleSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("WindowState"), state);
}

bool ConsoleSettings::isToolBarEnabled() const
{
    return settings_.value(QStringLiteral("ToolBar"), true).toBool();
}

void ConsoleSettings::setToolBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("ToolBar"), enable);
}

bool ConsoleSettings::isStatusBarEnabled() const
{
    return settings_.value(QStringLiteral("StatusBar"), true).toBool();
}

void ConsoleSettings::setStatusBarEnabled(bool enable)
{
    settings_.setValue(QStringLiteral("StatusBar"), enable);
}

proto::auth::SessionType ConsoleSettings::sessionType()
{
    return static_cast<proto::auth::SessionType>(
        settings_.value(QStringLiteral("SessionType"),
                        proto::auth::SESSION_TYPE_DESKTOP_MANAGE).toInt());
}

void ConsoleSettings::setSessionType(proto::auth::SessionType session_type)
{
    settings_.setValue(QStringLiteral("SessionType"), session_type);
}

} // namespace aspia
