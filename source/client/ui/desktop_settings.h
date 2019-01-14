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

#ifndef ASPIA_CLIENT__UI__DESKTOP_SETTINGS_H
#define ASPIA_CLIENT__UI__DESKTOP_SETTINGS_H

#include <QSettings>

#include "base/macros_magic.h"

namespace client {

class DesktopSettings
{
public:
    DesktopSettings();

    bool scaling() const;
    void setScaling(bool enable);

    bool autoScrolling() const;
    void setAutoScrolling(bool enable);

    bool sendKeyCombinations() const;
    void setSendKeyCombinations(bool enable);

private:
    QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSettings);
};

} // namespace client

#endif // ASPIA_CLIENT__UI__DESKTOP_SETTINGS_H
