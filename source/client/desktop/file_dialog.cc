//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/desktop/file_dialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

#include "client/settings.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString startDirectory()
{
    const QString directory = Settings().lastDirectory();
    if (directory.isEmpty() || !QDir(directory).exists())
        return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    return directory;
}

//--------------------------------------------------------------------------------------------------
void rememberDirectory(const QString& file_path)
{
    Settings().setLastDirectory(QFileInfo(file_path).absolutePath());
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString FileDialog::getOpenFileName(QWidget* parent, const QString& caption, const QString& filter,
                                    QString* selected_filter)
{
    const QString file_path =
        QFileDialog::getOpenFileName(parent, caption, startDirectory(), filter, selected_filter);
    if (!file_path.isEmpty())
        rememberDirectory(file_path);

    return file_path;
}

//--------------------------------------------------------------------------------------------------
QString FileDialog::getSaveFileName(QWidget* parent, const QString& caption, const QString& filter,
                                    QString* selected_filter)
{
    const QString file_path =
        QFileDialog::getSaveFileName(parent, caption, startDirectory(), filter, selected_filter);
    if (!file_path.isEmpty())
        rememberDirectory(file_path);

    return file_path;
}
