//
// PROJECT:         Aspia
// FILE:            system_info/category_logical_drive.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_logical_drive.h"
#include "system_info/category_logical_drive.pb.h"
#include "base/files/logical_drive_enumerator.h"
#include "base/strings/unicode.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* DriveTypeToString(proto::LogicalDrive::DriveType value)
{
    switch (value)
    {
        case proto::LogicalDrive::DRIVE_TYPE_LOCAL:
            return "Local Disk";

        case proto::LogicalDrive::DRIVE_TYPE_REMOVABLE:
            return "Removable Disk";

        case proto::LogicalDrive::DRIVE_TYPE_REMOTE:
            return "Remote Disk";

        case proto::LogicalDrive::DRIVE_TYPE_CDROM:
            return "Optical Disk";

        case proto::LogicalDrive::DRIVE_TYPE_RAM:
            return "RAM Disk";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryLogicalDrives::CategoryLogicalDrives()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryLogicalDrives::Name() const
{
    return "Logical Drives";
}

Category::IconId CategoryLogicalDrives::Icon() const
{
    return IDI_DRIVE;
}

const char* CategoryLogicalDrives::Guid() const
{
    return "{8F6D959F-C9E1-41E4-9D97-A12F650B489F}";
}

void CategoryLogicalDrives::Parse(Table& table, const std::string& data)
{
    proto::LogicalDrive message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Drive", 50)
                     .AddColumn("Label", 80)
                     .AddColumn("Drive Type", 80)
                     .AddColumn("File System", 80)
                     .AddColumn("Total Size", 80)
                     .AddColumn("Used Space", 80)
                     .AddColumn("Free Space", 80)
                     .AddColumn("% Free", 50)
                     .AddColumn("Volume Serial", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::LogicalDrive::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.drive_letter()));
        row.AddValue(Value::String(item.drive_label()));
        row.AddValue(Value::String(DriveTypeToString(item.drive_type())));
        row.AddValue(Value::String(item.file_system()));
        row.AddValue(Value::MemorySizeInBytes(item.total_size()));

        uint64_t used_space = item.total_size() - item.free_space();
        row.AddValue(Value::MemorySizeInBytes(used_space));

        row.AddValue(Value::MemorySizeInBytes(item.free_space()));

        uint64_t free_percent = (item.free_space() * 100ULL) / item.total_size();
        row.AddValue(Value::Number(free_percent, "%"));

        row.AddValue(Value::String(item.volume_serial()));
    }
}

std::string CategoryLogicalDrives::Serialize()
{
    proto::LogicalDrive message;

    LogicalDriveEnumerator enumerator;

    for (;;)
    {
        FilePath path = enumerator.Next();

        if (path.empty())
            break;

        LogicalDriveEnumerator::DriveInfo drive_info = enumerator.GetInfo();

        proto::LogicalDrive::Item* item = message.add_item();
        item->set_drive_letter(drive_info.Path().u8string());
        item->set_drive_label(UTF8fromUNICODE(drive_info.VolumeName()));

        switch (drive_info.Type())
        {
            case LogicalDriveEnumerator::DriveInfo::DriveType::FIXED:
                item->set_drive_type(proto::LogicalDrive::DRIVE_TYPE_LOCAL);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::REMOVABLE:
                item->set_drive_type(proto::LogicalDrive::DRIVE_TYPE_REMOVABLE);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::REMOTE:
                item->set_drive_type(proto::LogicalDrive::DRIVE_TYPE_REMOTE);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::CDROM:
                item->set_drive_type(proto::LogicalDrive::DRIVE_TYPE_CDROM);
                break;

            case LogicalDriveEnumerator::DriveInfo::DriveType::RAM:
                item->set_drive_type(proto::LogicalDrive::DRIVE_TYPE_RAM);
                break;

            default:
                break;
        }

        item->set_file_system(UTF8fromUNICODE(drive_info.FileSystem()));
        item->set_total_size(drive_info.TotalSpace());
        item->set_free_space(drive_info.FreeSpace());
        item->set_volume_serial(drive_info.VolumeSerial());
    }

    return message.SerializeAsString();
}

} // namespace aspia
