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

namespace aspia {

DesktopSettings::DesktopSettings()
    : settings_(QSettings::UserScope, QStringLiteral("aspia"), QStringLiteral("client"))
{
    // Nothing
}

bool DesktopSettings::scaling() const
{
    return settings_.value(QStringLiteral("desktop/scaling"), false).toBool();
}

void DesktopSettings::setScaling(bool enable)
{
    settings_.setValue(QStringLiteral("desktop/scaling"), enable);
}

bool DesktopSettings::autoScrolling() const
{
    return settings_.value(QStringLiteral("desktop/auto_scrolling"), true).toBool();
}

void DesktopSettings::setAutoScrolling(bool enable)
{
    settings_.setValue(QStringLiteral("desktop/auto_scrolling"), enable);
}

bool DesktopSettings::sendKeyCombinations() const
{
    return settings_.value(QStringLiteral("desktop/send_key_combinations"), true).toBool();
}

void DesktopSettings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(QStringLiteral("desktop/send_key_combinations"), enable);
}

} // namespace aspia
