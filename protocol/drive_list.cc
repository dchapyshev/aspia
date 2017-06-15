//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/drive_list.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/drive_list.h"
#include "base/drive_enumerator.h"
#include "base/unicode.h"
#include "base/path.h"

namespace aspia {

std::unique_ptr<proto::DriveList> CreateDriveList()
{
    std::unique_ptr<proto::DriveList> drive_list(new proto::DriveList());

    DriveEnumerator enumerator;

    for (;;)
    {
        std::wstring path = enumerator.Next();

        if (path.empty())
            break;

        proto::DriveListItem* item = drive_list->add_item();

        item->set_path(UTF8fromUNICODE(path));

        DriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        item->set_name(UTF8fromUNICODE(drive_info.VolumeName()));

        switch (drive_info.Type())
        {
            case DriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_type(proto::DriveListItem::CDROM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_type(proto::DriveListItem::REMOVABLE);
                break;

            case DriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_type(proto::DriveListItem::FIXED);
                break;

            case DriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_type(proto::DriveListItem::RAM);
                break;

            case DriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_type(proto::DriveListItem::REMOTE);
                break;

            default:
                item->set_type(proto::DriveListItem::UNKNOWN);
                break;
        }
    }

    std::wstring path;

    if (GetPathW(PathKey::DIR_USER_HOME, path))
    {
        proto::DriveListItem* item = drive_list->add_item();

        item->set_type(proto::DriveListItem::HOME_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    if (GetPathW(PathKey::DIR_USER_DESKTOP, path))
    {
        proto::DriveListItem* item = drive_list->add_item();

        item->set_type(proto::DriveListItem::DESKTOP_FOLDER);
        item->set_path(UTF8fromUNICODE(path));
    }

    return drive_list;
}

} // namespace aspia
