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

#ifndef CLIENT_DESKTOP_FILE_TRANSFER_FILE_REMOVE_WIDGET_H
#define CLIENT_DESKTOP_FILE_TRANSFER_FILE_REMOVE_WIDGET_H

#include <QWidget>

#include <memory>

#include "client/file_remover.h"

#if defined(Q_OS_WINDOWS)
class TaskbarProgress;
#endif // defined(Q_OS_WINDOWS)

namespace Ui {
class FileRemoveWidget;
} // namespace Ui

class FileRemoveWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit FileRemoveWidget(QWidget* parent = nullptr);
    ~FileRemoveWidget() final;

    void reset();
    void requestStop();
    bool isFinished() const { return finished_; }

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
    void sig_finished();

private:
    void updateTaskbarWindow();

    std::unique_ptr<Ui::FileRemoveWidget> ui;
    std::unique_ptr<QFontMetrics> label_metrics_;
    bool queue_building_ = true;
    bool stopping_ = false;
    bool finished_ = false;

#if defined(Q_OS_WINDOWS)
    TaskbarProgress* taskbar_progress_ = nullptr;
#endif // defined(Q_OS_WINDOWS)

    Q_DISABLE_COPY_MOVE(FileRemoveWidget)
};

#endif // CLIENT_DESKTOP_FILE_TRANSFER_FILE_REMOVE_WIDGET_H
