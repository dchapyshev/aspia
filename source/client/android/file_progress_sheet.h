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

#ifndef CLIENT_ANDROID_FILE_PROGRESS_SHEET_H
#define CLIENT_ANDROID_FILE_PROGRESS_SHEET_H

#include "common/android/dialog.h"

class QProgressBar;

// Modal progress card for a running file transfer or removal: a caption with the current item and a
// progress bar. The cancel button reports sig_cancel; the owner closes the sheet when the task ends.
class FileProgressSheet final : public Dialog
{
    Q_OBJECT

public:
    explicit FileProgressSheet(const QString& title, QWidget* parent = nullptr);
    ~FileProgressSheet() final;

    void setCurrentItem(const QString& text);
    void setProgress(int percentage);

signals:
    void sig_cancel();

private:
    Label* item_ = nullptr;
    QProgressBar* bar_ = nullptr;

    Q_DISABLE_COPY_MOVE(FileProgressSheet)
};

#endif // CLIENT_ANDROID_FILE_PROGRESS_SHEET_H
