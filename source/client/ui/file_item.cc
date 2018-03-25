//
// PROJECT:         Aspia
// FILE:            client/ui/file_item.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_item.h"

#include <QCoreApplication>
#include <QDateTime>

#include "client/file_platform_util.h"

namespace aspia {

namespace {

QString sizeToString(qint64 size)
{
    static const qint64 kTB = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    static const qint64 kGB = 1024ULL * 1024ULL * 1024ULL;
    static const qint64 kMB = 1024ULL * 1024ULL;
    static const qint64 kKB = 1024ULL;

    QString units;
    qint64 divider;

    if (size >= kTB)
    {
        units = QCoreApplication::tr("TB");
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = QCoreApplication::tr("GB");
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = QCoreApplication::tr("MB");
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = QCoreApplication::tr("kB");
        divider = kKB;
    }
    else
    {
        units = QCoreApplication::tr("B");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

} // namespace

FileItem::FileItem(const proto::file_transfer::FileList::Item& item)
    : is_directory_(item.is_directory()),
      size_(item.size()),
      last_modified_(item.modification_time())
{
    name_ = QString::fromUtf8(item.name().c_str(), item.name().size());

    setFlags(flags() | Qt::ItemIsEditable);
    setText(0, name_);

    if (item.is_directory())
    {
        setIcon(0, FilePlatformUtil::directoryIcon());
        setText(2, QCoreApplication::tr("Folder"));
    }
    else
    {
        QPair<QIcon, QString> type_info = FilePlatformUtil::fileTypeInfo(name_);

        setIcon(0, type_info.first);
        setText(1, sizeToString(item.size()));
        setText(2, type_info.second);
        
    }

    setText(3, QDateTime::fromTime_t(
        item.modification_time()).toString(Qt::DefaultLocaleShortDate));
}

FileItem::FileItem(const QString& directory_name)
    : is_directory_(true)
{
    setFlags(flags() | Qt::ItemIsEditable);
    setText(0, directory_name);
    setIcon(0, FilePlatformUtil::directoryIcon());
}

FileItem::~FileItem() = default;

QString FileItem::initialName() const
{
    return name_;
}

QString FileItem::currentName() const
{
    return text(0);
}

bool FileItem::isDirectory() const
{
    return is_directory_;
}

qint64 FileItem::fileSize() const
{
    return size_;
}

time_t FileItem::lastModified() const
{
    return last_modified_;
}

bool FileItem::operator<(const QTreeWidgetItem& other) const
{
    const FileItem* file_item = reinterpret_cast<const FileItem*>(&other);

    // Directories are always higher than files.
    if (is_directory_ && !file_item->is_directory_)
        return false;
    else if (!is_directory_ && file_item->is_directory_)
        return false;

    int column = treeWidget()->sortColumn();
    if (column == 1) // Sorting by size.
        return size_ < file_item->size_;
    else if (column == 3) // Sorting by date/time.
        return last_modified_ < file_item->last_modified_;
    else
        return text(column).toLower() < other.text(column).toLower();
}

} // namespace aspia
