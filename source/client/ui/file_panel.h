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

#ifndef ASPIA_CLIENT__UI__FILE_PANEL_H_
#define ASPIA_CLIENT__UI__FILE_PANEL_H_

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "protocol/file_transfer_session.pb.h"
#include "ui_file_panel.h"

namespace aspia {

class FilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() = default;

    void setPanelName(const QString& name);

    QString currentPath() const { return current_path_; }
    void setCurrentPath(const QString& path);

signals:
    void newRequest(FileRequest* request);
    void removeItems(FilePanel* sender, const QList<FileRemover::Item>& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender, const QList<FileTransfer::Item>& items);

public slots:
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);
    void refresh();

protected:
    // QWidget implementation.
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onAddressItemChanged(int index);
    void onFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onFileSelectionChanged();
    void onFileNameChanged(FileItem* file_item);
    void onFileContextMenu(const QPoint& point);

    void toChildFolder(const QString& child_name);
    void toParentFolder();
    void addFolder();
    void removeSelected();
    void sendSelected();

private:
    QString addressItemPath(int index) const;
    void updateDrives(const proto::file_transfer::DriveList& list);
    void updateFiles(const proto::file_transfer::FileList& list);
    int selectedFilesCount();

    Ui::FilePanel ui;
    QString current_path_;

    DISALLOW_COPY_AND_ASSIGN(FilePanel);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__FILE_PANEL_H_
