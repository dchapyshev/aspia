//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/directory_list.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/directory_list.h"
#include "base/file_enumerator.h"
#include "base/unicode.h"

namespace aspia {

std::unique_ptr<proto::DirectoryList> CreateDirectoryList(const std::wstring& path)
{
    std::unique_ptr<proto::DirectoryList> directory_list(new proto::DirectoryList());

    directory_list->set_path(UTF8fromUNICODE(path));

    FileEnumerator enumerator(path,
                              false,
                              FileEnumerator::FILES | FileEnumerator::DIRECTORIES);

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DirectoryListItem* item = directory_list->add_item();
        FileEnumerator::FileInfo file_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(file_info.GetName()));

        if (!file_info.IsDirectory())
        {
            item->set_type(proto::DirectoryListItem::FILE);
            item->set_size(file_info.GetSize());
        }
        else
        {
            item->set_type(proto::DirectoryListItem::DIRECTORY);
        }
    }

    return directory_list;
}

} // namespace aspia
