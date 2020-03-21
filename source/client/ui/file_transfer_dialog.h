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

#ifndef CLIENT__UI__FILE_TRANSFER_DIALOG_H
#define CLIENT__UI__FILE_TRANSFER_DIALOG_H

#include "build/build_config.h"
#include "client/file_transfer.h"
#include "client/file_transfer_window.h"
#include "ui_file_transfer_dialog.h"

#if defined(OS_WIN)
class QWinTaskbarProgress;
#endif

namespace client {

class FileTransferDialog
    : public QDialog,
      public FileTransferWindow
{
    Q_OBJECT

public:
    explicit FileTransferDialog(QWidget* parent = nullptr);
    ~FileTransferDialog();

    std::shared_ptr<FileTransferWindowProxy> windowProxy() { return transfer_window_proxy_; }

    // FileTransferWindow implementation.
    void start(std::shared_ptr<FileTransferProxy> transfer_proxy) override;
    void stop() override;
    void setCurrentItem(const std::string& source_path, const std::string& target_path) override;
    void setCurrentProgress(int total, int current) override;
    void errorOccurred(const FileTransfer::Error& error) override;

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QString errorToMessage(const FileTransfer::Error& error);

    Ui::FileTransferDialog ui;

    std::shared_ptr<FileTransferProxy> transfer_proxy_;
    std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy_;
    std::unique_ptr<QFontMetrics> label_metrics_;

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
