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

#include "client/ui/desktop_settings.h"

namespace client {

namespace {

const QString kScaleParam = QStringLiteral("Desktop/Scale");
const QString kAutoScrollingParam = QStringLiteral("Desktop/AutoScrolling");
const QString kSendKeyCombinationsParam = QStringLiteral("Desktop/SendKeyCombinations");

} // namespace

DesktopSettings::DesktopSettings()
    : settings_(QSettings::IniFormat,
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

int DesktopSettings::scale() const
{
    int result = settings_.value(kScaleParam, -1).toInt();

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
    settings_.setValue(kScaleParam, value);
}

bool DesktopSettings::autoScrolling() const
{
    return settings_.value(kAutoScrollingParam, true).toBool();
}

void DesktopSettings::setAutoScrolling(bool enable)
{
    settings_.setValue(kAutoScrollingParam, enable);
}

bool DesktopSettings::sendKeyCombinations() const
{
    return settings_.value(kSendKeyCombinationsParam, true).toBool();
}

void DesktopSettings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(kSendKeyCombinationsParam, enable);
}

} // namespace client
