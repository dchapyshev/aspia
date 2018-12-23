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

#include "base/xml_settings.h"

namespace aspia {

DesktopSettings::DesktopSettings()
    : settings_(XmlSettings::registerFormat(),
                QSettings::UserScope,
                QLatin1String("aspia"),
                QLatin1String("client"))
{
    // Nothing
}

bool DesktopSettings::scaling() const
{
    return settings_.value(QLatin1String("Desktop/Scaling"), false).toBool();
}

void DesktopSettings::setScaling(bool enable)
{
    settings_.setValue(QLatin1String("Desktop/Scaling"), enable);
}

bool DesktopSettings::autoScrolling() const
{
    return settings_.value(QLatin1String("Desktop/AutoScrolling"), true).toBool();
}

void DesktopSettings::setAutoScrolling(bool enable)
{
    settings_.setValue(QLatin1String("Desktop/AutoScrolling"), enable);
}

bool DesktopSettings::sendKeyCombinations() const
{
    return settings_.value(QLatin1String("Desktop/SendKeyCombinations"), true).toBool();
}

void DesktopSettings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(QLatin1String("Desktop/SendKeyCombinations"), enable);
}

} // namespace aspia
