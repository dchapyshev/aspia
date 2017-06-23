//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_helpers.h"
#include "ui/get_stock_icon.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

#include <clocale>
#include <ctime>

namespace aspia {

std::wstring TimeToString(time_t time)
{
    tm* local_time = std::localtime(&time);

    if (!local_time)
        return std::wstring();

    // Set the locale obtained from system.
    std::setlocale(LC_TIME, "");

    wchar_t string[128];
    if (!std::wcsftime(string, _countof(string), L"%x %X", local_time))
        return std::wstring();

    return string;
}

HICON GetDriveIcon(proto::DriveListItem::Type drive_type)
{
    SHSTOCKICONID icon_id;

    switch (drive_type)
    {
        case proto::DriveListItem::CDROM:
            icon_id = SIID_DRIVECD;
            break;

        case proto::DriveListItem::FIXED:
            icon_id = SIID_DRIVEFIXED;
            break;

        case proto::DriveListItem::REMOVABLE:
            icon_id = SIID_DRIVEREMOVE;
            break;

        case proto::DriveListItem::REMOTE:
            icon_id = SIID_DRIVENET;
            break;

        case proto::DriveListItem::RAM:
            icon_id = SIID_DRIVERAM;
            break;

        case proto::DriveListItem::DESKTOP_FOLDER:
            icon_id = SIID_DESKTOP;
            break;

        case proto::DriveListItem::HOME_FOLDER:
        default:
            icon_id = SIID_FOLDER;
            break;
    }

    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (FAILED(GetStockIconInfo(icon_id, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
        return nullptr;

    return icon_info.hIcon;
}

HICON GetComputerIcon()
{
    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (FAILED(GetStockIconInfo(SIID_DESKTOPPC, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
        return nullptr;

    return icon_info.hIcon;
}

HICON GetDirectoryIcon()
{
    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (FAILED(GetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
        return nullptr;

    return icon_info.hIcon;
}

HICON GetFileIcon(const std::wstring& file_name)
{
    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(file_name.c_str(),
                   FILE_ATTRIBUTE_NORMAL,
                   &file_info,
                   sizeof(file_info),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);

    return file_info.hIcon;
}

std::wstring GetDriveDisplayName(const proto::DriveListItem& item)
{
    switch (item.type())
    {
        case proto::DriveListItem::HOME_FOLDER:
            return UiModule::Current().string(IDS_FT_HOME_FOLDER);

        case proto::DriveListItem::DESKTOP_FOLDER:
            return UiModule::Current().string(IDS_FT_DESKTOP_FOLDER);

        case proto::DriveListItem::CDROM:
        case proto::DriveListItem::FIXED:
        case proto::DriveListItem::REMOVABLE:
        case proto::DriveListItem::REMOTE:
        case proto::DriveListItem::RAM:
        {
            if (!item.name().empty())
            {
                return StringPrintfW(L"%s (%s)",
                                     UNICODEfromUTF8(item.path()).c_str(),
                                     UNICODEfromUTF8(item.name()).c_str());
            }
        }
        break;
    }

    return UNICODEfromUTF8(item.path());
}

std::wstring GetDriveDescription(proto::DriveListItem::Type type)
{
    const UiModule& module = UiModule::Current();

    switch (type)
    {
        case proto::DriveListItem::HOME_FOLDER:
            return module.string(IDS_FT_DRIVE_DESC_HOME);

        case proto::DriveListItem::DESKTOP_FOLDER:
            return module.string(IDS_FT_DRIVE_DESC_DESKTOP);

        case proto::DriveListItem::CDROM:
            return module.string(IDS_FT_DRIVE_DESC_CDROM);

        case proto::DriveListItem::FIXED:
            return module.string(IDS_FT_DRIVE_DESC_FIXED);

        case proto::DriveListItem::REMOVABLE:
            return module.string(IDS_FT_DRIVE_DESC_REMOVABLE);

        case proto::DriveListItem::REMOTE:
            return module.string(IDS_FT_DRIVE_DESC_REMOTE);

        case proto::DriveListItem::RAM:
            return module.string(IDS_FT_DRIVE_DESC_RAM);

        default:
            return module.string(IDS_FT_DRIVE_DESC_UNKNOWN);
    }
}

std::wstring GetDirectoryTypeString(const std::wstring& dir_name)
{
    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(dir_name.c_str(),
                   FILE_ATTRIBUTE_DIRECTORY,
                   &file_info,
                   sizeof(file_info),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME);

    return file_info.szTypeName;
}

std::wstring GetFileTypeString(const std::wstring& file_name)
{
    SHFILEINFO file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(file_name.c_str(),
                   FILE_ATTRIBUTE_NORMAL,
                   &file_info,
                   sizeof(file_info),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME);

    return file_info.szTypeName;
}

std::wstring SizeToString(uint64_t size)
{
    static const uint64_t kTB = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    static const uint64_t kGB = 1024ULL * 1024ULL * 1024ULL;
    static const uint64_t kMB = 1024ULL * 1024ULL;
    static const uint64_t kKB = 1024ULL;

    const UiModule& module = UiModule().Current();

    std::wstring units;
    uint64_t divider;

    if (size >= kTB)
    {
        units = module.string(IDS_FT_SIZE_TBYTES);
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = module.string(IDS_FT_SIZE_GBYTES);
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = module.string(IDS_FT_SIZE_MBYTES);
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = module.string(IDS_FT_SIZE_KBYTES);
        divider = kKB;
    }
    else
    {
        units = module.string(IDS_FT_SIZE_BYTES);
        divider = 1;
    }

    return StringPrintfW(L"%.2f %s",
                         double(size) / double(divider),
                         units.c_str());
}

} // namespace aspia
