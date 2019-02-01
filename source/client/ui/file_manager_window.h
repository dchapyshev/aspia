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

#ifndef CLIENT__UI__FILE_MANAGER_WINDOW_H
#define CLIENT__UI__FILE_MANAGER_WINDOW_H

#include "client/ui/client_window.h"
#include "ui_file_manager_window.h"

namespace client {

class FileRemoveDialog;
class FileTransferDialog;

class FileManagerWindow : public ClientWindow
{
    Q_OBJECT

public:
    FileManagerWindow(const ConnectData& connect_data, QWidget* parent);
    ~FileManagerWindow();

    QByteArray saveState() const;
    void restoreState(const QByteArray& state);

public slots:
    void refresh();

protected:
    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

    // SessionWindow implementation.
    void sessionStarted() override;
    void sessionError() override;

private slots:
    void removeItems(FilePanel* sender, const QList<FileRemover::Item>& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender,
                      const QString& target_folder,
                      const QList<FileTransfer::Item>& items);
    void onPathChanged(FilePanel* sender, const QString& path);

private:
    void transferItems(FileTransfer::Type type,
                       const QString& source_path,
                       const QString& target_path,
                       const QList<FileTransfer::Item>& items);
    static QString createWindowTitle(const ConnectData& connect_data);

    Ui::FileManagerWindow ui;

    QPointer<FileRemoveDialog> remove_dialog_;
    QPointer<FileTransferDialog> transfer_dialog_;

    DISALLOW_COPY_AND_ASSIGN(FileManagerWindow);
};

} // namespace client

#endif // CLIENT__UI__FILE_MANAGER_WINDOW_H
