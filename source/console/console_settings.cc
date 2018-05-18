//
// PROJECT:         Aspia
// FILE:            console/console_settings.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_settings.h"

#include <QDir>

namespace aspia {

ConsoleSettings::ConsoleSettings()
    : settings_(QSettings::UserScope, QStringLiteral("Aspia"), QStringLiteral("Console"))
{
    // Nothing
}

ConsoleSettings::~ConsoleSettings() = default;

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

} // namespace aspia
