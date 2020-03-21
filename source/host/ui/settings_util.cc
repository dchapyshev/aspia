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

#include "host/ui/settings_util.h"

#include "base/xml_settings.h"
#include "host/system_settings.h"

#include <QMessageBox>

namespace host {

// static
bool SettingsUtil::importFromFile(const std::filesystem::path& path, bool silent, QWidget* parent)
{
    bool result = copySettings(path, SystemSettings().filePath(), silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully imported."),
                                 QMessageBox::Ok);
    }

    return result;
}

// static
bool SettingsUtil::exportToFile(const std::filesystem::path& path, bool silent, QWidget* parent)
{
    bool result = copySettings(SystemSettings().filePath(), path, silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully exported."),
                                 QMessageBox::Ok);
    }

    return result;
}

// static
bool SettingsUtil::copySettings(const std::filesystem::path& source_path,
                                const std::filesystem::path& target_path,
                                bool silent,
                                QWidget* parent)
{
    base::XmlSettings::Map settings_map;

    if (!base::XmlSettings::readFile(source_path, settings_map))
    {
        if (!silent)
        {
            QMessageBox::warning(
                parent,
                tr("Warning"),
                tr("Unable to read the source file: the file is damaged or has an unknown format."),
                QMessageBox::Ok);
        }

        return false;
    }

    if (!base::XmlSettings::writeFile(target_path, settings_map))
    {
        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Unable to write the target file."),
                                 QMessageBox::Ok);
        }

        return false;
    }

    return true;
}

} // namespace host
