//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_manager_helpers.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_manager_helpers.h"
#include "ui/get_stock_icon.h"
#include "ui/resource.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"

#include <clocale>

namespace aspia {

HICON GetDriveIcon(proto::file_transfer::DriveList::Item::Type drive_type)
{
    SHSTOCKICONID icon_id;

    switch (drive_type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_CDROM:
            icon_id = SIID_DRIVECD;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_FIXED:
            icon_id = SIID_DRIVEFIXED;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_REMOVABLE:
            icon_id = SIID_DRIVEREMOVE;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_REMOTE:
            icon_id = SIID_DRIVENET;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_RAM:
            icon_id = SIID_DRIVERAM;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            icon_id = SIID_DESKTOP;
            break;

        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
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

HICON GetFileIcon(const std::experimental::filesystem::path& file_name)
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

CString GetDriveDisplayName(const proto::file_transfer::DriveList::Item& item)
{
    CString name;

    switch (item.type())
    {
        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            name.LoadStringW(IDS_FT_HOME_FOLDER);
            return name;

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            name.LoadStringW(IDS_FT_DESKTOP_FOLDER);
            return name;

        default:
        {
            if (!item.name().empty())
            {
                name.Format(L"%s (%s)",
                            UNICODEfromUTF8(item.path()).c_str(),
                            UNICODEfromUTF8(item.name()).c_str());
                return name;
            }
        }
        break;
    }

    return UNICODEfromUTF8(item.path()).c_str();
}

CString GetDriveDescription(proto::file_transfer::DriveList::Item::Type type)
{
    CString desc;

    switch (type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_HOME);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_DESKTOP);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_CDROM:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_CDROM);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_FIXED:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_FIXED);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_REMOVABLE:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_REMOVABLE);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_REMOTE:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_REMOTE);
            break;

        case proto::file_transfer::DriveList::Item::TYPE_RAM:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_RAM);
            break;

        default:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_UNKNOWN);
            break;
    }

    return desc;
}

CString GetFileTypeString(const std::experimental::filesystem::path& file_name)
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

    wchar_t units[128];
    uint64_t divider;

    if (size >= kTB)
    {
        AtlLoadString(IDS_FT_SIZE_TBYTES, units, _countof(units));
        divider = kTB;
    }
    else if (size >= kGB)
    {
        AtlLoadString(IDS_FT_SIZE_GBYTES, units, _countof(units));
        divider = kGB;
    }
    else if (size >= kMB)
    {
        AtlLoadString(IDS_FT_SIZE_MBYTES, units, _countof(units));
        divider = kMB;
    }
    else if (size >= kKB)
    {
        AtlLoadString(IDS_FT_SIZE_KBYTES, units, _countof(units));
        divider = kKB;
    }
    else
    {
        AtlLoadString(IDS_FT_SIZE_BYTES, units, _countof(units));
        divider = 1;
    }

    return StringPrintf(L"%.2f %s",
                        static_cast<double>(size) / static_cast<double>(divider),
                        units);
}

} // namespace aspia
