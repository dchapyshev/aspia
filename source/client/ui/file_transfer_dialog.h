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

#ifndef CLIENT__UI__FILE_TRANSFER_DIALOG_H
#define CLIENT__UI__FILE_TRANSFER_DIALOG_H

#include "build/build_config.h"
#include "client/file_transfer.h"
#include "ui_file_transfer_dialog.h"

#if defined(OS_WIN)
class QWinTaskbarProgress;
#endif

namespace client {

class FileTransferDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileTransferDialog(QWidget* parent = nullptr);
    ~FileTransferDialog();

public slots:
    void setCurrentItem(const QString& source_path, const QString& target_path);
    void setProgress(int total, int current);
    void showError(FileTransfer* transfer, FileTransfer::Error error_type, const QString& message);
    void onTransferFinished();

signals:
    void transferCanceled();

protected:
    void closeEvent(QCloseEvent* event);

private:
    Ui::FileTransferDialog ui;

    QScopedPointer<QFontMetrics> label_metrics_;

#if defined(OS_WIN)
    QWinTaskbarProgress* taskbar_progress_ = nullptr;
#endif

    bool task_queue_building_ = true;
    bool closing_ = false;
    bool finished_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDialog);
};

} // namespace client

#endif // CLIENT__UI__FILE_TRANSFER_DIALOG_H
