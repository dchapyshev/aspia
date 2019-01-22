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

#ifndef CLIENT__UI__FILE_PANEL_H
#define CLIENT__UI__FILE_PANEL_H

#include "client/file_remover.h"
#include "client/file_transfer.h"
#include "proto/file_transfer_session.pb.h"
#include "ui_file_panel.h"

namespace client {

class FilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() = default;

    void setPanelName(const QString& name);
    void setMimeType(const QString& mime_type);
    void setTransferAllowed(bool allowed);
    void setTransferEnabled(bool enabled);

    QString currentPath() const { return ui.address_bar->currentPath(); }

    QByteArray saveState() const;
    void restoreState(const QByteArray& state);

signals:
    void newRequest(common::FileRequest* request);
    void removeItems(FilePanel* sender, const QList<FileRemover::Item>& items);
    void sendItems(FilePanel* sender, const QList<FileTransfer::Item>& items);
    void receiveItems(FilePanel* sender,
                      const QString& folder,
                      const QList<FileTransfer::Item>& items);
    void pathChanged(FilePanel* sender, const QString& path);

public slots:
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);
    void refresh();

protected:
    // QWidget implementation.
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onPathChanged(const QString& path);
    void onListItemActivated(const QModelIndex& index);
    void onListSelectionChanged();
    void onListContextMenu(const QPoint& point);
    void onNameChangeRequest(const QString& old_name, const QString& new_name);
    void onCreateFolderRequest(const QString& name);

    void toChildFolder(const QString& child_name);
    void toParentFolder();
    void addFolder();
    void removeSelected();
    void sendSelected();

private:
    void sendRequest(common::FileRequest* request);

    Ui::FilePanel ui;

    bool transfer_allowed_ = false;
    bool transfer_enabled_ = false;

    DISALLOW_COPY_AND_ASSIGN(FilePanel);
};

} // namespace client

#endif // CLIENT__UI__FILE_PANEL_H
