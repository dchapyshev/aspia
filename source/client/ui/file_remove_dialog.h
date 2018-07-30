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

#ifndef ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H_
#define ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H_

#include "client/file_remover.h"
#include "ui_file_remove_dialog.h"

namespace aspia {

class FileRemoveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileRemoveDialog(QWidget* parent);
    ~FileRemoveDialog() = default;

public slots:
    void setProgress(const QString& current_item, int percentage);
    void showError(FileRemover* remover, FileRemover::Actions actions, const QString& message);

private:
    Ui::FileRemoveDialog ui;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveDialog);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__FILE_REMOVE_DIALOG_H_
