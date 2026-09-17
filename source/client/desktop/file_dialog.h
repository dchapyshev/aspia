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

#ifndef CLIENT_DESKTOP_FILE_DIALOG_H
#define CLIENT_DESKTOP_FILE_DIALOG_H

#include <QString>

class QWidget;

// Left to itself a dialog opens in the working directory of the process, which is the directory of
// the binary. These open where the user was last time, and in his home directory until then.
class FileDialog
{
public:
    static QString getOpenFileName(QWidget* parent, const QString& caption, const QString& filter,
                                   QString* selected_filter = nullptr);

    static QString getSaveFileName(QWidget* parent, const QString& caption, const QString& filter,
                                   QString* selected_filter = nullptr);
};

#endif // CLIENT_DESKTOP_FILE_DIALOG_H
