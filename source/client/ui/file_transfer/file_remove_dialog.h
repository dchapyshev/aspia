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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_REMOVE_DIALOG_H
#define CLIENT_UI_FILE_TRANSFER_FILE_REMOVE_DIALOG_H

#include "client/file_remover.h"
#include "ui_file_remove_dialog.h"

#if defined(Q_OS_WINDOWS)
#include "common/ui/taskbar_progress.h"
#endif // defined(Q_OS_WINDOWS)

namespace client {

class FileRemoveDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FileRemoveDialog(QWidget* parent);
    ~FileRemoveDialog() final;

public slots:
    void start();
    void stop();
    void setCurrentProgress(const QString& name, int percentage);
    void errorOccurred(const QString& path,
                       proto::file_transfer::ErrorCode error_code,
                       quint32 available_actions);

signals:
    void sig_stop();
    void sig_action(FileRemover::Action action);

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) final;

private:
    Ui::FileRemoveDialog ui;
    std::unique_ptr<QFontMetrics> label_metrics_;
    bool stopped_ = false;

#if defined(Q_OS_WINDOWS)
    common::TaskbarProgress* taskbar_progress_ = nullptr;
#endif // defined(Q_OS_WINDOWS)

    Q_DISABLE_COPY(FileRemoveDialog)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_REMOVE_DIALOG_H
