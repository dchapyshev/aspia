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

#include "client/ui/address_bar_model.h"

#include "host/file_platform_util.h"

namespace aspia {

namespace {

const quintptr kComputerItem = 1;
const quintptr kDriveItem = 2;
const quintptr kCurrentFolderItem = 3;

enum Column
{
    COLUMN_NAME        = 0,
    COLUMN_TYPE        = 1,
    COLUMN_TOTAL_SPACE = 2,
    COLUMN_FREE_SPACE  = 3,
    COLUMN_COUNT       = 4
};

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

AddressBarModel::AddressBarModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    // Nothing
}

void AddressBarModel::setDriveList(const proto::file_transfer::DriveList& list)
{
    drives_.clear();

    for (int i = 0; i < list.item_size(); ++i)
    {
        const proto::file_transfer::DriveList::Item& item = list.item(i);

        Drive drive;
        drive.icon        = FilePlatformUtil::driveIcon(item.type());
        drive.path        = normalizePath(QString::fromStdString(item.path()));
        drive.type        = typeToString(item.type());
        drive.total_space = item.total_space();
        drive.free_space  = item.free_space();

        switch (item.type())
        {
            case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
                drive.name = tr("Home Folder");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
                drive.name = tr("Desktop");
                break;

            default:
            {
                drive.name = QString::fromStdString(item.name());
                if (drive.name.isEmpty())
                {
                    drive.name = drive.path;
                    break;
                }

                drive.name = QString("%1 (%2)").arg(drive.path).arg(drive.name);
            }
            break;
        }

        drives_.append(drive);
    }

    emit dataChanged(QModelIndex(), QModelIndex());
}

QModelIndex AddressBarModel::setCurrentPath(const QString& path)
{
    QModelIndex index;

    current_path_.clear();

    if (path == computerPath())
    {
        index = computerIndex();
    }
    else
    {
        QString normalized_path = normalizePath(path);

        for (int i = 0; i < drives_.count(); ++i)
        {
            const QString& drive_path = drives_.at(i).path;

            if (drive_path.compare(normalized_path, Qt::CaseInsensitive) == 0)
            {
                index = createIndex(i, 0, kDriveItem);
                break;
            }
        }

        if (!index.isValid())
        {
            current_path_ = normalized_path;
            index = currentFolderIndex();
        }
    }

    emit dataChanged(QModelIndex(), QModelIndex());
    return index;
}

QString AddressBarModel::pathAt(const QModelIndex& index) const
{
    return data(index, Qt::UserRole).toString();
}

QModelIndex AddressBarModel::computerIndex() const
{
    if (current_path_.isEmpty())
        return createIndex(0, 0, kComputerItem);
    else
        return createIndex(1, 0, kComputerItem);
}

QModelIndex AddressBarModel::currentFolderIndex() const
{
    if (current_path_.isEmpty())
        return QModelIndex();

    return createIndex(0, 0, kCurrentFolderItem);
}

// static
const QString& AddressBarModel::computerPath()
{
    static const QString kComputerPath = QStringLiteral(":computer");
    return kComputerPath;
}

QModelIndex AddressBarModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        if (current_path_.isEmpty())
            return computerIndex();
        else if (row == 0)
            return currentFolderIndex();
        else if (row == 1)
            return computerIndex();
        else
            return QModelIndex();
    }

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.internalId() != kComputerItem)
        return QModelIndex();

    return createIndex(row, column, kDriveItem);
}

QModelIndex AddressBarModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    if (child.internalId() == kDriveItem)
        return createIndex(0, 0, kComputerItem);

    return QModelIndex();
}

int AddressBarModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        if (current_path_.isEmpty())
            return 1;

        return 2;
    }

    if (parent.column() > 0)
        return 0;

    if (parent.internalId() != kComputerItem)
        return 0;

    return drives_.count();
}

int AddressBarModel::columnCount(const QModelIndex& /* parent */) const
{
    return COLUMN_COUNT;
}

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
                    return QIcon(QStringLiteral(":/icon/computer.png"));
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
        }
    }
    else if (index.internalId() == kCurrentFolderItem)
    {
        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (index.column() == COLUMN_NAME)
                    return QIcon(QStringLiteral(":/icon/folder.png"));
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
                    return drive.name;
                }
                else if (index.column() == COLUMN_TYPE)
                {
                    return drive.type;
                }
                else if (index.column() == COLUMN_TOTAL_SPACE && drive.total_space >= 0)
                {
                    return sizeToString(drive.total_space);
                }
                else if (index.column() == COLUMN_FREE_SPACE && drive.free_space >= 0)
                {
                    return sizeToString(drive.free_space);
                }
            }
            break;

            case Qt::UserRole:
                return drive.path;
        }
    }

    return QVariant();
}

bool AddressBarModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole)
        return false;

    emit pathIndexChanged(setCurrentPath(value.toString()));
    return true;
}

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

        case COLUMN_TOTAL_SPACE:
            return tr("Total Space");

        case COLUMN_FREE_SPACE:
            return tr("Free Space");

        default:
            return QVariant();
    }
}

Qt::ItemFlags AddressBarModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags default_flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.internalId() == kComputerItem)
        return default_flags;

    return default_flags | Qt::ItemNeverHasChildren;
}

bool AddressBarModel::insertRows(int row, int count, const QModelIndex& parent)
{
    return true;
}

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

        default:
            return tr("Unknown Drive");
    }
}

// static
QString AddressBarModel::sizeToString(int64_t size)
{
    static const int64_t kKB = 1024LL;
    static const int64_t kMB = kKB * 1024LL;
    static const int64_t kGB = kMB * 1024LL;
    static const int64_t kTB = kGB * 1024LL;

    QString units;
    int64_t divider;

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

} // namespace aspia
