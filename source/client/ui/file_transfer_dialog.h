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

#ifndef ASPIA_CLIENT__UI__FILE_TRANSFER_DIALOG_H_
#define ASPIA_CLIENT__UI__FILE_TRANSFER_DIALOG_H_

#include "client/file_transfer.h"
#include "ui_file_transfer_dialog.h"

namespace aspia {

class FileTransferDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileTransferDialog(QWidget* parent = nullptr);
    ~FileTransferDialog() = default;

public slots:
    void setCurrentItem(const QString& source_path, const QString& target_path);
    void setProgress(int total, int current);
    void showError(FileTransfer* transfer, FileTransfer::Error error_type, const QString& message);

private:
    Ui::FileTransferDialog ui;

    bool task_queue_building_ = true;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDialog);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__FILE_TRANSFER_DIALOG_H_
