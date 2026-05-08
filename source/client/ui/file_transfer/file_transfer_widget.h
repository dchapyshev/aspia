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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_WIDGET_H
#define CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_WIDGET_H

#include <QWidget>

#include <memory>

#include "client/file_transfer.h"

#if defined(Q_OS_WINDOWS)
class TaskbarProgress;
#endif

namespace Ui {
class FileTransferWidget;
} // namespace Ui

class FileTransferWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit FileTransferWidget(QWidget* parent = nullptr);
    ~FileTransferWidget() final;

    void reset();
    void requestStop();
    bool isFinished() const { return finished_; }

public slots:
    void start();
    void stop();
    void setCurrentItem(const QString& source_path, const QString& target_path);
    void setCurrentProgress(int total, int current);
    void setCurrentSpeed(qint64 speed);
    void errorOccurred(const FileTransfer::Error& error);

signals:
    void sig_action(FileTransfer::Error::Type error_type, FileTransfer::Error::Action action);
    void sig_stop();
    void sig_finished();

private:
    QString errorToMessage(const FileTransfer::Error& error);
    void updateTaskbarWindow();

    std::unique_ptr<Ui::FileTransferWidget> ui;
    std::unique_ptr<QFontMetrics> label_metrics_;

#if defined(Q_OS_WINDOWS)
    TaskbarProgress* taskbar_progress_ = nullptr;
#endif

    bool task_queue_building_ = true;
    bool stopping_ = false;
    bool finished_ = false;

    Q_DISABLE_COPY_MOVE(FileTransferWidget)
};

#endif // CLIENT_UI_FILE_TRANSFER_FILE_TRANSFER_WIDGET_H
