//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "common/file_platform_util.h"
#include "proto/file_transfer.pb.h"

#include <QAbstractItemModel>

namespace client {

class AddressBarModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit AddressBarModel(QObject* parent = nullptr);

    void setDriveList(const proto::DriveList& list);
    QModelIndex setCurrentPath(const QString& path);
    QString previousPath() const { return previous_path_; }
    QString pathAt(const QModelIndex& index) const;

    bool isComputerPath(const QString& path) const;
    bool isDrivePath(const QString& path) const;

    QModelIndex computerIndex() const;
    QModelIndex currentFolderIndex() const;

    static const QString& computerPath();

    // QAbstractItemModel implementation.
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool insertRows(int row, int count, const QModelIndex& parent) override;

signals:
    void sig_pathIndexChanged(const QModelIndex& index);
    void sig_invalidPathEntered();

protected:
    static QString typeToString(proto::DriveList::Item::Type type);
    static QString sizeToString(int64_t size);

private:
    struct Drive
    {
        proto::DriveList::Item::Type type = proto::DriveList::Item::TYPE_UNKNOWN;
        QIcon icon;
        QString name;
        QString path;
        int64_t total_space = 0;
        int64_t free_space = 0;
    };

    QString current_path_ = computerPath();
    QString previous_path_;
    QList<Drive> drives_;

    DISALLOW_COPY_AND_ASSIGN(AddressBarModel);
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_ADDRESS_BAR_MODEL_H
