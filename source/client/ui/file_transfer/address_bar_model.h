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

#ifndef CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_MODEL_H
#define CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_MODEL_H

#include <QAbstractItemModel>
#include <QIcon>

#include "proto/file_transfer.h"

namespace client {

class AddressBarModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit AddressBarModel(QObject* parent = nullptr);

    void setDriveList(const proto::file_transfer::DriveList& list);
    QModelIndex setCurrentPath(const QString& path);
    QString previousPath() const { return previous_path_; }
    QString pathAt(const QModelIndex& index) const;

    bool isComputerPath(const QString& path) const;
    bool isDrivePath(const QString& path) const;

    QModelIndex computerIndex() const;
    QModelIndex currentFolderIndex() const;

    static const QString& computerPath();

    // QAbstractItemModel implementation.
    QModelIndex index(int row, int column, const QModelIndex& parent) const final;
    QModelIndex parent(const QModelIndex& child) const final;
    int rowCount(const QModelIndex& parent) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    bool setData(const QModelIndex& index, const QVariant& value, int role) final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;
    Qt::ItemFlags flags(const QModelIndex& index) const final;
    bool insertRows(int row, int count, const QModelIndex& parent) final;

signals:
    void sig_pathIndexChanged(const QModelIndex& index);
    void sig_invalidPathEntered();

protected:
    static QString typeToString(proto::file_transfer::DriveList::Item::Type type);
    static QString sizeToString(qint64 size);

private:
    struct Drive
    {
        proto::file_transfer::DriveList::Item::Type type = proto::file_transfer::DriveList::Item::TYPE_UNKNOWN;
        QIcon icon;
        QString name;
        QString path;
    };

    QString current_path_ = computerPath();
    QString previous_path_;
    QList<Drive> drives_;

    Q_DISABLE_COPY(AddressBarModel)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_MODEL_H
