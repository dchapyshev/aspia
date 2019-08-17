//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef QT_BASE__QT_XML_SETTINGS_H
#define QT_BASE__QT_XML_SETTINGS_H

#include "base/macros_magic.h"

#include <QSettings>

namespace qt_base {

class QtXmlSettings
{
public:
    static QSettings::Format format();

    static bool readFunc(QIODevice& device, QSettings::SettingsMap& map);
    static bool writeFunc(QIODevice& device, const QSettings::SettingsMap& map);

private:
    DISALLOW_COPY_AND_ASSIGN(QtXmlSettings);
};

} // namespace qt_base

#endif // QT_BASE__QT_XML_SETTINGS_H
