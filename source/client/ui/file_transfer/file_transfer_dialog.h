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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_DIALOG_H
#define CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_DIALOG_H

#include "client/file_transfer.h"
#include "ui_file_transfer_dialog.h"

#if defined(Q_OS_WINDOWS)
#include "common/ui/taskbar_progress.h"
#endif

namespace client {

class FileTransferDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FileTransferDialog(QWidget* parent = nullptr);
    ~FileTransferDialog() final;

public slots:
    void start();
    void stop();
    void setCurrentItem(const QString& source_path, const QString& target_path);
    void setCurrentProgress(int total, int current);
    void setCurrentSpeed(qint64 speed);
    void errorOccurred(const client::FileTransfer::Error& error);

signals:
    void sig_action(client::FileTransfer::Error::Type error_type,
                    client::FileTransfer::Error::Action action);
    void sig_stop();

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private:
    QString errorToMessage(const FileTransfer::Error& error);
    QString speedToString(qint64 speed);

    Ui::FileTransferDialog ui;
    std::unique_ptr<QFontMetrics> label_metrics_;

#if defined(Q_OS_WINDOWS)
    common::TaskbarProgress* taskbar_progress_ = nullptr;
#endif

    bool task_queue_building_ = true;
    bool closing_ = false;
    bool finished_ = false;

    Q_DISABLE_COPY(FileTransferDialog)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_DIALOG_H
