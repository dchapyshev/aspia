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

#include "build/build_config.h"
#include "client/file_transfer.h"
#include "ui_file_transfer_dialog.h"

// Removed completely in qt6.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(OS_WIN)
class QWinTaskbarProgress;
#endif
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
    void setCurrentItem(const std::string& source_path, const std::string& target_path);
    void setCurrentProgress(int total, int current);
    void setCurrentSpeed(int64_t speed);
    void errorOccurred(const FileTransfer::Error& error);

signals:
    void sig_action(FileTransfer::Error::Type error_type, FileTransfer::Error::Action action);
    void sig_stop();

protected:
    // QDialog implementation.
    void keyPressEvent(QKeyEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private:
    QString errorToMessage(const FileTransfer::Error& error);
    QString speedToString(int64_t speed);

    Ui::FileTransferDialog ui;
    std::unique_ptr<QFontMetrics> label_metrics_;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(OS_WIN)
    QWinTaskbarProgress* taskbar_progress_ = nullptr;
#endif
#endif

    bool task_queue_building_ = true;
    bool closing_ = false;
    bool finished_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDialog);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_DIALOG_H
