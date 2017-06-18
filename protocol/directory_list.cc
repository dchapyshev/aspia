//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/directory_list.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/directory_list.h"

namespace aspia {

std::unique_ptr<proto::DirectoryList> CreateDirectoryList(
    const std::experimental::filesystem::path& path)
{
    std::unique_ptr<proto::DirectoryList> directory_list(new proto::DirectoryList());

    directory_list->set_path(path.u8string());

    std::error_code code;

    for (auto& entry : std::experimental::filesystem::directory_iterator(path, code))
    {
        proto::DirectoryListItem* item = directory_list->add_item();

        item->set_name(entry.path().filename().u8string());

        std::experimental::filesystem::file_time_type time =
            std::experimental::filesystem::last_write_time(entry.path(), code);

        item->set_modified(decltype(time)::clock::to_time_t(time));

        if (entry.status().type() == std::experimental::filesystem::file_type::directory)
        {
            item->set_type(proto::DirectoryListItem::DIRECTORY);
        }
        else
        {
            item->set_type(proto::DirectoryListItem::FILE);

            uintmax_t size = std::experimental::filesystem::file_size(entry.path(), code);
            if (size != -1)
                item->set_size(size);
        }
    }

    return directory_list;
}

} // namespace aspia
