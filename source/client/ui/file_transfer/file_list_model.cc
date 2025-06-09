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

#include "client/ui/file_transfer/file_list_model.h"

#include "client/ui/file_transfer/file_mime_data.h"
#include "common/file_platform_util.h"

#include <QDateTime>
#include <QLocale>

namespace client {

namespace {

enum Column
{
    COLUMN_NAME       = 0,
    COLUMN_SIZE       = 1,
    COLUMN_TYPE       = 2,
    COLUMN_LAST_WRITE = 3,
    COLUMN_COUNT      = 4
};

//--------------------------------------------------------------------------------------------------
template<class T>
void sortByName(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(),
              [order](const typename T::value_type& f1, const typename T::value_type& f2)
    {
        const QString& f1_name = f1.name;
        const QString& f2_name = f2.name;

        if (order == Qt::AscendingOrder)
            return f1_name.toLower() < f2_name.toLower();
        else
            return f1_name.toLower() > f2_name.toLower();
    });
}

//--------------------------------------------------------------------------------------------------
template<class T>
void sortBySize(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(),
              [order](const typename T::value_type& f1, const typename T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.size < f2.size;
        else
            return f1.size > f2.size;
    });
}

//--------------------------------------------------------------------------------------------------
template<class T>
void sortByType(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(),
              [order](const typename T::value_type& f1, const typename T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.type < f2.type;
        else
            return f1.type > f2.type;
    });
}

//--------------------------------------------------------------------------------------------------
template<class T>
void sortByTime(T& list, Qt::SortOrder order)
{
    std::sort(list.begin(), list.end(),
              [order](const typename T::value_type& f1, const typename T::value_type& f2)
    {
        if (order == Qt::AscendingOrder)
            return f1.last_write < f2.last_write;
        else
            return f1.last_write > f2.last_write;
    });
}

} // namespace

//--------------------------------------------------------------------------------------------------
FileListModel::FileListModel(QObject* parent)
    : QAbstractItemModel(parent),
      dir_icon_(common::FilePlatformUtil::directoryIcon()),
      dir_type_(tr("Folder"))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void FileListModel::setMimeType(const QString& mime_type)
{
    mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
void FileListModel::setFileList(const proto::file_transfer::List& list)
{
    clear();

    if (!list.item_size())
        return;

    beginInsertRows(QModelIndex(), 0, list.item_size() - 1);

    for (int i = 0; i < list.item_size(); ++i)
    {
        const proto::file_transfer::List::Item& item = list.item(i);

        if (item.is_directory())
        {
            Folder folder;
            folder.name       = QString::fromStdString(item.name());
            folder.last_write = item.modification_time();

            folder_items_.append(folder);
        }
        else
        {
            File file;
            file.name       = QString::fromStdString(item.name());
            file.last_write = item.modification_time();
            file.size       = static_cast<qint64>(item.size());

            std::pair<QIcon, QString> file_info = common::FilePlatformUtil::fileTypeInfo(file.name);
            file.icon = file_info.first;
            file.type = file_info.second;

            file_items_.append(file);
        }
    }

    sortItems(current_column_, current_order_);

    endInsertRows();
}

//--------------------------------------------------------------------------------------------------
void FileListModel::setSortOrder(int column, Qt::SortOrder order)
{
    current_column_ = column;
    current_order_ = order;
}

//--------------------------------------------------------------------------------------------------
void FileListModel::clear()
{
    if (folder_items_.isEmpty() && file_items_.isEmpty())
        return;

    beginRemoveRows(QModelIndex(), 0, folder_items_.count() + file_items_.count() - 1);

    folder_items_.clear();
    file_items_.clear();

    endRemoveRows();
}

//--------------------------------------------------------------------------------------------------
bool FileListModel::isFolder(const QModelIndex& index) const
{
    return index.row() < folder_items_.count();
}

//--------------------------------------------------------------------------------------------------
QString FileListModel::nameAt(const QModelIndex& index) const
{
    if (isFolder(index))
        return folder_items_.at(index.row()).name;

    return file_items_.at(index.row() - folder_items_.count()).name;
}

//--------------------------------------------------------------------------------------------------
qint64 FileListModel::sizeAt(const QModelIndex& index) const
{
    if (isFolder(index))
        return 0;

    return file_items_.at(index.row() - folder_items_.count()).size;
}

//--------------------------------------------------------------------------------------------------
QModelIndex FileListModel::createFolder()
{
    int row = folder_items_.count();

    if (row > 0)
    {
        QString last_folder = folder_items_.last().name;
        if (last_folder.isEmpty())
            return QModelIndex();
    }

    beginInsertRows(QModelIndex(), row, row);
    folder_items_.append(Folder());
    endInsertRows();

    return createIndex(row, COLUMN_NAME);
}

//--------------------------------------------------------------------------------------------------
QModelIndex FileListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
        return QModelIndex();

    return createIndex(row, column);
}

//--------------------------------------------------------------------------------------------------
QModelIndex FileListModel::parent(const QModelIndex& /* child */) const
{
    return QModelIndex();
}

//--------------------------------------------------------------------------------------------------
int FileListModel::rowCount(const QModelIndex& /* parent */) const
{
    return folder_items_.count() + file_items_.count();
}

//--------------------------------------------------------------------------------------------------
int FileListModel::columnCount(const QModelIndex& /* parent */) const
{
    return COLUMN_COUNT;
}

//--------------------------------------------------------------------------------------------------
QVariant FileListModel::data(const QModelIndex& index, int role) const
{
    int column = index.column();
    int row = index.row();

    if (!index.isValid() || (folder_items_.count() + file_items_.count()) <= row)
        return QVariant();

    if (isFolder(index))
    {
        const Folder& folder = folder_items_.at(row);

        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (column == COLUMN_NAME)
                    return dir_icon_;
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                switch (column)
                {
                    case COLUMN_NAME:
                        return folder.name;

                    case COLUMN_LAST_WRITE:
                        return timeToString(folder.last_write);

                    case COLUMN_TYPE:
                        return dir_type_;

                    default:
                        break;
                }
            }
            break;

            default:
                break;
        }
    }
    else
    {
        const File& file = file_items_.at(row - folder_items_.count());

        switch (role)
        {
            case Qt::DecorationRole:
            {
                if (column == COLUMN_NAME)
                    return file.icon;
            }
            break;

            case Qt::DisplayRole:
            case Qt::EditRole:
            {
                switch (column)
                {
                    case COLUMN_NAME:
                        return file.name;

                    case COLUMN_SIZE:
                        return sizeToString(file.size);

                    case COLUMN_TYPE:
                        return file.type;

                    case COLUMN_LAST_WRITE:
                        return timeToString(file.last_write);

                    default:
                        break;
                }
            }
            break;

            default:
                break;
        }
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
bool FileListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || (folder_items_.count() + file_items_.count()) <= index.row())
        return false;

    if (role != Qt::EditRole)
        return false;

    if (index.column() != COLUMN_NAME)
        return false;

    QString old_name = nameAt(index);
    QString new_name = value.toString();

    if (old_name.isEmpty())
    {
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        folder_items_.removeLast();
        endRemoveRows();

        emit sig_createFolderRequest(new_name);
    }
    else
    {
        emit sig_nameChangeRequest(old_name, new_name);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return QVariant();

    switch (section)
    {
        case COLUMN_NAME:
            return tr("Name");

        case COLUMN_SIZE:
            return tr("Size");

        case COLUMN_TYPE:
            return tr("Type");

        case COLUMN_LAST_WRITE:
            return tr("Modified");

        default:
            return QVariant();
    }
}

//--------------------------------------------------------------------------------------------------
QStringList FileListModel::mimeTypes() const
{
    return QStringList() << mime_type_;
}

//--------------------------------------------------------------------------------------------------
QMimeData* FileListModel::mimeData(const QModelIndexList& indexes) const
{
    QList<FileTransfer::Item> file_list;

    for (const auto& index : indexes)
    {
        if (index.column() == COLUMN_NAME)
            file_list.push_back(FileTransfer::Item(nameAt(index), sizeAt(index), isFolder(index)));
    }

    if (file_list.empty())
        return nullptr;

    FileMimeData* mime_data = new FileMimeData();
    mime_data->setMimeType(mime_type_);
    mime_data->setFileList(file_list);
    mime_data->setSource(this);

    return mime_data;
}

//--------------------------------------------------------------------------------------------------
bool FileListModel::canDropMimeData(const QMimeData* data, Qt::DropAction /* action */,
                                    int /* row */, int /* column */,
                                    const QModelIndex& /* parent */) const
{
    if (!data->hasFormat(mime_type_))
        return false;

    const FileMimeData* mime_data = dynamic_cast<const FileMimeData*>(data);
    if (!mime_data)
        return false;

    const FileListModel* source = mime_data->source();

    if (!source || source == this || source->mimeType() != mimeType())
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool FileListModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                 int row, int column, const QModelIndex& parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    const FileMimeData* mime_data = dynamic_cast<const FileMimeData*>(data);
    if (!mime_data)
        return false;

    QString folder;
    if (parent.isValid() && isFolder(parent))
        folder = nameAt(parent);

    emit sig_fileListDropped(folder, mime_data->fileList());
    return true;
}

//--------------------------------------------------------------------------------------------------
Qt::DropActions FileListModel::supportedDropActions() const
{
    return Qt::CopyAction;
}

//--------------------------------------------------------------------------------------------------
Qt::DropActions FileListModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

//--------------------------------------------------------------------------------------------------
Qt::ItemFlags FileListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    Qt::ItemFlags default_flags =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;

    switch (index.column())
    {
        case COLUMN_NAME:
        {
            if (isFolder(index))
            {
                return Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled |
                    default_flags;
            }
            else
            {
                return Qt::ItemIsEditable | Qt::ItemIsDragEnabled | default_flags;
            }
        }

        default:
            return Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | default_flags;
    }
}

//--------------------------------------------------------------------------------------------------
void FileListModel::sort(int column, Qt::SortOrder order)
{
    sortItems(column, order);
    emit dataChanged(QModelIndex(), QModelIndex());
}

//--------------------------------------------------------------------------------------------------
void FileListModel::sortItems(int column, Qt::SortOrder order)
{
    current_order_ = order;
    current_column_ = column;

    switch (column)
    {
        case COLUMN_NAME:
            sortByName(folder_items_, order);
            sortByName(file_items_, order);
            break;

        case COLUMN_SIZE:
            sortBySize(file_items_, order);
            break;

        case COLUMN_TYPE:
            sortByType(file_items_, order);
            break;

        case COLUMN_LAST_WRITE:
            sortByTime(folder_items_, order);
            sortByTime(file_items_, order);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString FileListModel::sizeToString(qint64 size)
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

//--------------------------------------------------------------------------------------------------
// static
QString FileListModel::timeToString(time_t time)
{
    return QLocale::system().toString(QDateTime::fromSecsSinceEpoch(time), QLocale::ShortFormat);
}

} // namespace client
