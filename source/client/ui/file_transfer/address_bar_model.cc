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

#include "client/ui/file_transfer/address_bar_model.h"

#include "base/logging.h"
#include "common/file_platform_util.h"

namespace client {

namespace {

const quintptr kComputerItem = 1;
const quintptr kDriveItem = 2;
const quintptr kCurrentFolderItem = 3;

enum Column
{
    COLUMN_NAME  = 0,
    COLUMN_TYPE  = 1,
    COLUMN_COUNT = 2
};

//--------------------------------------------------------------------------------------------------
QString normalizePath(const QString& path)
{
    if (path.isEmpty())
        return QString();

    QString normalized_path = path;

    normalized_path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (!normalized_path.endsWith(QLatin1Char('/')))
        normalized_path += QLatin1Char('/');

    return normalized_path;
}

} // namespace

//--------------------------------------------------------------------------------------------------
AddressBarModel::AddressBarModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void AddressBarModel::setDriveList(const proto::file_transfer::DriveList& list)
{
    drives_.clear();

    for (int i = 0; i < list.item_size(); ++i)
    {
        const proto::file_transfer::DriveList::Item& item = list.item(i);

        Drive drive;
        drive.icon = common::FilePlatformUtil::driveIcon(item.type());
        drive.path = normalizePath(QString::fromStdString(item.path()));
        drive.type = item.type();

        switch (item.type())
        {
            case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
                drive.name = tr("Home Folder");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
                drive.name = tr("Desktop");
                break;

            default:
                break;
        }

        drives_.append(drive);
    }

    emit dataChanged(QModelIndex(), QModelIndex());
}

//--------------------------------------------------------------------------------------------------
QModelIndex AddressBarModel::setCurrentPath(const QString& path)
{
    QModelIndex index;

    LOG(INFO) << "Previous path:" << previous_path_;
    LOG(INFO) << "New path:" << path;

    previous_path_ = current_path_;

    if (path == computerPath())
    {
        current_path_ = computerPath();
        index = computerIndex();
    }
    else
    {
        QString normalized_path = normalizePath(path);

        if (!common::FilePlatformUtil::isValidPath(normalized_path))
        {
            LOG(ERROR) << "Invalid path entered:" << normalized_path;
            emit sig_invalidPathEntered();
            return QModelIndex();
        }
        else
        {
            current_path_ = normalized_path;

            for (int i = 0; i < drives_.count(); ++i)
            {
                const QString& drive_path = drives_.at(i).path;

                if (drive_path.compare(current_path_, Qt::CaseInsensitive) == 0)
                {
                    index = createIndex(i, 0, kDriveItem);
                    break;
                }
            }

            if (!index.isValid())
                index = currentFolderIndex();
        }
    }

    emit dataChanged(QModelIndex(), QModelIndex());
    return index;
}

//--------------------------------------------------------------------------------------------------
QString AddressBarModel::pathAt(const QModelIndex& index) const
{
    return data(index, Qt::UserRole).toString();
}

//--------------------------------------------------------------------------------------------------
bool AddressBarModel::isComputerPath(const QString& path) const
{
    return path == computerPath();
}

//--------------------------------------------------------------------------------------------------
bool AddressBarModel::isDrivePath(const QString& path) const
{
    QString normalized_path = normalizePath(path);

    for (const auto& drive : drives_)
    {
        if (normalized_path.compare(drive.path, Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
QModelIndex AddressBarModel::computerIndex() const
{
    if (isDrivePath(current_path_) || isComputerPath(current_path_))
        return createIndex(0, 0, kComputerItem);
    else
        return createIndex(1, 0, kComputerItem);
}

//--------------------------------------------------------------------------------------------------
QModelIndex AddressBarModel::currentFolderIndex() const
{
    if (isDrivePath(current_path_) || isComputerPath(current_path_))
        return QModelIndex();

    return createIndex(0, 0, kCurrentFolderItem);
}

//--------------------------------------------------------------------------------------------------
// static
const QString& AddressBarModel::computerPath()
{
    static const QString kComputerPath = QStringLiteral(":computer");
    return kComputerPath;
}

//--------------------------------------------------------------------------------------------------
QModelIndex AddressBarModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        if (isDrivePath(current_path_) || isComputerPath(current_path_))
        {
            if (row == 0)
                return computerIndex();
        }
        else
        {
            if (row == 0)
                return currentFolderIndex();
            else if (row == 1)
                return computerIndex();
        }

        return QModelIndex();
    }

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.internalId() != kComputerItem)
        return QModelIndex();

    return createIndex(row, column, kDriveItem);
}

//--------------------------------------------------------------------------------------------------
QModelIndex AddressBarModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    if (child.internalId() == kDriveItem)
        return createIndex(0, 0, kComputerItem);

    return QModelIndex();
}

//--------------------------------------------------------------------------------------------------
int AddressBarModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        if (isDrivePath(current_path_))
            return 1;

        return 2;
    }

    if (parent.column() > 0)
        return 0;

    if (parent.internalId() != kComputerItem)
        return 0;

    return drives_.count();
}

//--------------------------------------------------------------------------------------------------
int AddressBarModel::columnCount(const QModelIndex& /* parent */) const
{
    return COLUMN_COUNT;
}

//--------------------------------------------------------------------------------------------------
QVariant AddressBarModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.internalId() == kComputerItem)
    {
        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (index.column() == COLUMN_NAME)
                    return QIcon(QStringLiteral(":/img/computer.svg"));
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                if (index.column() == COLUMN_NAME)
                    return tr("Computer");
            }
            break;

            case Qt::UserRole:
                return computerPath();

            default:
                break;
        }
    }
    else if (index.internalId() == kCurrentFolderItem)
    {
        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (index.column() == COLUMN_NAME)
                    return QIcon(QStringLiteral(":/img/folder.svg"));
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                if (index.column() == COLUMN_NAME)
                    return current_path_;
            }
            break;

            case Qt::UserRole:
                return current_path_;

            default:
                break;
        }
    }
    else if (index.internalId() == kDriveItem)
    {
        const Drive& drive = drives_.at(index.row());

        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (index.column() == COLUMN_NAME)
                    return drive.icon;
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                if (index.column() == COLUMN_NAME)
                {
                    if (drive.type == proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER ||
                        drive.type == proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER)
                    {
                        return drive.name;
                    }

                    if (role == Qt::EditRole || drive.name.isEmpty())
                        return drive.path;

                    return QString("%1 (%2)").arg(drive.path, drive.name);
                }

                if (index.column() == COLUMN_TYPE)
                    return typeToString(drive.type);
            }
            break;

            case Qt::UserRole:
                return drive.path;

            default:
                break;
        }
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
bool AddressBarModel::setData(const QModelIndex& /* index */, const QVariant& value, int role)
{
    if (role != Qt::EditRole)
        return false;

    QModelIndex current = setCurrentPath(value.toString());
    if (!current.isValid())
        current = setCurrentPath(previous_path_);

    emit sig_pathIndexChanged(current);
    return true;
}

//--------------------------------------------------------------------------------------------------
QVariant AddressBarModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();

    switch (section)
    {
        case COLUMN_NAME:
            return tr("Name");

        case COLUMN_TYPE:
            return tr("Type");

        default:
            return QVariant();
    }
}

//--------------------------------------------------------------------------------------------------
Qt::ItemFlags AddressBarModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags default_flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.internalId() == kComputerItem)
        return default_flags;

    return default_flags | Qt::ItemNeverHasChildren;
}

//--------------------------------------------------------------------------------------------------
bool AddressBarModel::insertRows(int /* row */, int /* count */, const QModelIndex& /* parent */)
{
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
QString AddressBarModel::typeToString(proto::file_transfer::DriveList::Item::Type type)
{
    switch (type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_CDROM:
            return tr("Optical Drive");

        case proto::file_transfer::DriveList::Item::TYPE_REMOVABLE:
            return tr("Removable Drive");

        case proto::file_transfer::DriveList::Item::TYPE_FIXED:
            return tr("Fixed Drive");

        case proto::file_transfer::DriveList::Item::TYPE_REMOTE:
            return tr("Network Drive");

        case proto::file_transfer::DriveList::Item::TYPE_RAM:
            return tr("RAM Drive");

        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            return tr("Home Folder");

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            return tr("Desktop Folder");

        case proto::file_transfer::DriveList::Item::TYPE_ROOT_DIRECTORY:
            return tr("Root Directory");

        default:
            return tr("Unknown Drive");
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString AddressBarModel::sizeToString(qint64 size)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (size >= kTB)
    {
        units = tr("TB");
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = tr("GB");
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = tr("MB");
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = tr("kB");
        divider = kKB;
    }
    else
    {
        units = tr("B");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

} // namespace client
