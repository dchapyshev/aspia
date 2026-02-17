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

#ifndef CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_H
#define CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_H

#include <QComboBox>

#include "proto/file_transfer.h"

class QLineEdit;
class QTreeView;

namespace client {

class AddressBarModel;

class AddressBar final : public QComboBox
{
    Q_OBJECT

public:
    explicit AddressBar(QWidget* parent = nullptr);

    void setDriveList(const proto::file_transfer::DriveList& list);
    void setCurrentPath(const QString& path);
    QString currentPath() const;
    QString previousPath() const;
    QString pathAt(const QModelIndex& index) const;
    bool hasCurrentPath() const;

    // QComboBox implementation.
    void showPopup() final;

signals:
    void sig_pathChanged(const QString& path);

private slots:
    void onPathIndexChanged(const QModelIndex& index);

private:
    QLineEdit* edit_;
    QTreeView* view_;
    AddressBarModel* model_;

    Q_DISABLE_COPY(AddressBar)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_H
