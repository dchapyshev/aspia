//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/settings/json_settings.h"
#include "base/logging.h"
#include "host/system_settings.h"

#include <QAbstractButton>
#include <QMessageBox>

namespace host {

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::importFromFile(const std::filesystem::path& path, bool silent, QWidget* parent)
{
    std::filesystem::path target_path = SystemSettings().filePath();

    LOG(LS_INFO) << "Import settings from '" << path << "' to '" << target_path << "'";

    bool result = copySettings(path, target_path, silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully imported."),
                                 QMessageBox::Ok);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::exportToFile(const std::filesystem::path& path, bool silent, QWidget* parent)
{
    std::filesystem::path source_path = SystemSettings().filePath();

    LOG(LS_INFO) << "Export settings from '" << source_path << "' to '" << path << "'";

    bool result = copySettings(source_path, path, silent, parent);

    if (!silent && result)
    {
        QMessageBox::information(parent,
                                 tr("Information"),
                                 tr("The configuration was successfully exported."),
                                 QMessageBox::Ok);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::copySettings(const std::filesystem::path& source_path,
                                const std::filesystem::path& target_path,
                                bool silent,
                                QWidget* parent)
{
    std::error_code error_code;
    if (!std::filesystem::exists(source_path, error_code))
    {
        LOG(LS_ERROR) << "Source settings file does't exist ("
                      << base::utf16FromLocal8Bit(error_code.message()) << ")";

        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Source settings file does not exist."),
                                 QMessageBox::Ok);
        }

        return false;
    }
    else
    {
        uintmax_t file_size = std::filesystem::file_size(source_path, error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Failed to get settings file size ("
                          << base::utf16FromLocal8Bit(error_code.message()) << ")";
        }

        LOG(LS_INFO) << "Source settings file exist (" << file_size << " bytes)";
    }

    base::JsonSettings::Map settings_map;
    if (!base::JsonSettings::readFile(source_path, settings_map))
    {
        LOG(LS_ERROR) << "Failed to read source file: " << source_path;

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
    else
    {
        LOG(LS_INFO) << "File read successfully: " << source_path;
    }

    if (std::filesystem::exists(target_path, error_code))
    {
        if (!silent)
        {
            QMessageBox message_box(QMessageBox::Warning,
                tr("Warning"),
                tr("The existing settings will be overwritten. Continue?"),
                QMessageBox::Yes | QMessageBox::No,
                parent);
            message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
            message_box.button(QMessageBox::No)->setText(tr("No"));

            if (message_box.exec() == QMessageBox::No)
            {
                LOG(LS_INFO) << "Copy settings canceled by user";
                return false;
            }
        }

        LOG(LS_INFO) << "Target settings file already exist and will be overwritten";
    }
    else
    {
        LOG(LS_INFO) << "Target settings file does't exist. New file will be created";
    }

    if (!base::JsonSettings::writeFile(target_path, settings_map))
    {
        LOG(LS_ERROR) << "Failed to write destination file: " << target_path;

        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Unable to write the target file."),
                                 QMessageBox::Ok);
        }

        return false;
    }
    else
    {
        LOG(LS_INFO) << "File written successfully: " << target_path;
    }

    return true;
}

} // namespace host
