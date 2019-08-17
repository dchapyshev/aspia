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

#include "client/ui/desktop_settings.h"

#include "qt_base/qt_xml_settings.h"

namespace client {

DesktopSettings::DesktopSettings()
    : settings_(qt_base::QtXmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

int DesktopSettings::scale() const
{
    int result = settings_.value(QStringLiteral("Desktop/Scale"), 100).toInt();

    switch (result)
    {
        case 100:
        case 90:
        case 80:
        case 70:
        case 60:
        case 50:
        case -1:
            return result;

        default:
            return 100;
    }
}

void DesktopSettings::setScale(int value)
{
    settings_.setValue(QStringLiteral("Desktop/Scale"), value);
}

bool DesktopSettings::autoScrolling() const
{
    return settings_.value(QStringLiteral("Desktop/AutoScrolling"), true).toBool();
}

void DesktopSettings::setAutoScrolling(bool enable)
{
    settings_.setValue(QStringLiteral("Desktop/AutoScrolling"), enable);
}

bool DesktopSettings::sendKeyCombinations() const
{
    return settings_.value(QStringLiteral("Desktop/SendKeyCombinations"), true).toBool();
}

void DesktopSettings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(QStringLiteral("Desktop/SendKeyCombinations"), enable);
}

} // namespace client
