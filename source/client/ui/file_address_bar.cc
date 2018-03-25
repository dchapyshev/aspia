//
// PROJECT:         Aspia
// FILE:            client/ui/file_address_bar.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_address_bar.h"

#include <QDebug>

#include "client/file_platform_util.h"

namespace aspia {

namespace {

QString normalizePath(const QString& path)
{
    QString normalized_path = path;

    normalized_path.replace('\\', '/');

    if (!normalized_path.endsWith('/'))
        normalized_path += '/';

    return normalized_path;
}

} // namespace

FileAddressBar::FileAddressBar(QWidget* parent)
    : QComboBox(parent)
{
    connect(this, SIGNAL(currentIndexChanged(int)), SLOT(itemChanged(int)));
    connect(this, SIGNAL(currentTextChanged(const QString&)), SLOT(textChanged(const QString&)));
}

FileAddressBar::~FileAddressBar() = default;

QString FileAddressBar::currentPath() const
{
    return itemPath(currentIndex());
}

QString FileAddressBar::itemPath(int index) const
{
    QString path = itemData(index).toString();
    if (path.isEmpty())
        path = itemText(index);

    return path;
}

void FileAddressBar::setCurrentPath(const QString& path)
{
    QString new_path = normalizePath(path);

    for (int i = 0; i < count(); ++i)
    {
        if (itemPath(i) == new_path)
        {
            setCurrentIndex(i);
            return;
        }
    }

    addItem(FilePlatformUtil::directoryIcon(), new_path);
    setCurrentIndex(count() - 1);
}

void FileAddressBar::updateDrives(const proto::file_transfer::DriveList& drive_list)
{
    clear();

    for (int i = 0; i < drive_list.item_size(); ++i)
    {
        const proto::file_transfer::DriveList::Item& item = drive_list.item(i);

        QString path = normalizePath(QString::fromUtf8(item.path().c_str(), item.path().size()));
        QIcon icon = FilePlatformUtil::driveIcon(item.type());

        switch (item.type())
        {
            case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
                insertItem(i, icon, tr("Home Folder"), path);
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
                insertItem(i, icon, tr("Desktop"), path);
                break;

            default:
                insertItem(i, icon, path, path);
                break;
        }
    }
}

void FileAddressBar::toChildFolder(const QString& child_name)
{
    setCurrentPath(currentPath() + child_name);
}

void FileAddressBar::toParentFolder()
{
    QString current_path = currentPath();

    int from = -1;
    if (current_path.endsWith('/'))
        from = -2;

    int last_slash = current_path.lastIndexOf('/', from);
    if (last_slash == -1)
        return;

    setCurrentPath(current_path.left(last_slash));
}

void FileAddressBar::refresh()
{
    emit addressChanged(currentPath());
}

void FileAddressBar::itemChanged(int index)
{
    QString new_path = itemData(index).toString();
    if (new_path.isEmpty())
        new_path = itemText(index);

    for (int i = 0; i < count(); ++i)
    {
        if (itemData(i).toString().isEmpty() && itemText(i) != itemText(index))
        {
            removeItem(i);
            break;
        }
    }

    emit addressChanged(new_path);
}

void FileAddressBar::textChanged(const QString& text)
{
    //emit addressChanged(text);
}

} // namespace aspia
