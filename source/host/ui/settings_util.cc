//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/xml_settings.h"
#include "host/system_settings.h"

#include <QAbstractButton>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>

namespace host {

//--------------------------------------------------------------------------------------------------
// static
bool SettingsUtil::importFromFile(const QString& path, bool silent, QWidget* parent)
{
    QString target_path = SystemSettings().filePath();

    LOG(LS_INFO) << "Import settings from" << path << "to" << target_path;

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
bool SettingsUtil::exportToFile(const QString& path, bool silent, QWidget* parent)
{
    QString source_path = SystemSettings().filePath();

    LOG(LS_INFO) << "Export settings from" << source_path << "to" << path;

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
bool SettingsUtil::copySettings(const QString& source_path,
                                const QString& target_path,
                                bool silent,
                                QWidget* parent)
{
    if (!QFileInfo::exists(source_path))
    {
        LOG(LS_ERROR) << "Source settings file does't exist";

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
        qint64 file_size = QFileInfo(source_path).size();
        LOG(LS_INFO) << "Source settings file exist (" << file_size << "bytes)";
    }

    QFile source_file(source_path);
    if (!source_file.open(QFile::ReadOnly))
    {
        LOG(LS_ERROR) << "Unable to open source file:" << source_file.errorString();

        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Unable to open the source file."),
                                 QMessageBox::Ok);
        }

        return false;
    }

    QSettings::SettingsMap settings_map;
    if (!base::XmlSettings::readFunc(source_file, settings_map))
    {
        LOG(LS_ERROR) << "Failed to read source file:" << source_path;

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
        LOG(LS_INFO) << "File read successfully:" << source_path;
    }

    if (QFileInfo::exists(target_path))
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

    QFile target_file(target_path);
    if (!target_file.open(QFile::ReadWrite))
    {
        LOG(LS_ERROR) << "Unable to open target file:" << target_file.errorString();

        if (!silent)
        {
            QMessageBox::warning(parent,
                                 tr("Warning"),
                                 tr("Unable to open the target file."),
                                 QMessageBox::Ok);
        }

        return false;
    }

    if (!base::XmlSettings::writeFunc(target_file, settings_map))
    {
        LOG(LS_ERROR) << "Failed to write destination file:" << target_path;

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
        LOG(LS_INFO) << "File written successfully:" << target_path;
    }

    return true;
}

} // namespace host
