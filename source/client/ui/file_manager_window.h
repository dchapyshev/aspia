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

#ifndef ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H_
#define ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H_

#include "base/macros_magic.h"
#include "client/connect_data.h"
#include "ui_file_manager_window.h"

namespace aspia {

class FileManagerWindow : public QWidget
{
    Q_OBJECT

public:
    FileManagerWindow(ConnectData* connect_data, QWidget* parent = nullptr);
    ~FileManagerWindow() = default;

public slots:
    void refresh();

signals:
    void windowClose();
    void localRequest(FileRequest* request);
    void remoteRequest(FileRequest* request);

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

private slots:
    void removeItems(FilePanel* sender, const QList<FileRemover::Item>& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender, const QList<FileTransfer::Item>& items);

private:
    void transferItems(FileTransfer::Type type,
                       const QString& source_path,
                       const QString& target_path,
                       const QList<FileTransfer::Item>& items);

    Ui::FileManagerWindow ui;

    DISALLOW_COPY_AND_ASSIGN(FileManagerWindow);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__FILE_MANAGER_WINDOW_H_
