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

// Removed completely in qt6.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
class QWinTaskbarProgress;
#endif
#endif

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
    void setCurrentProgress(const std::string& name, int percentage);
    void errorOccurred(const std::string& path,
                       proto::FileError error_code,
                       quint32 available_actions);

signals:
    void sig_stop();
    void sig_action(FileRemover::Action action);

protected:
    // QDialog implementation.
    void closeEvent(QCloseEvent* event) final;

private:
    Ui::FileRemoveDialog ui;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#if defined(Q_OS_WINDOWS)
    QWinTaskbarProgress* taskbar_progress_ = nullptr;
#endif
#endif

    std::unique_ptr<QFontMetrics> label_metrics_;
    bool stopped_ = false;

    DISALLOW_COPY_AND_ASSIGN(FileRemoveDialog);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_REMOVE_DIALOG_H
