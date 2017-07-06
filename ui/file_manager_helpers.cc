//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_helpers.h"
#include "ui/get_stock_icon.h"
#include "ui/resource.h"
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

HICON GetDriveIcon(proto::DriveList::Item::Type drive_type)
{
    SHSTOCKICONID icon_id;

    switch (drive_type)
    {
        case proto::DriveList::Item::CDROM:
            icon_id = SIID_DRIVECD;
            break;

        case proto::DriveList::Item::FIXED:
            icon_id = SIID_DRIVEFIXED;
            break;

        case proto::DriveList::Item::REMOVABLE:
            icon_id = SIID_DRIVEREMOVE;
            break;

        case proto::DriveList::Item::REMOTE:
            icon_id = SIID_DRIVENET;
            break;

        case proto::DriveList::Item::RAM:
            icon_id = SIID_DRIVERAM;
            break;

        case proto::DriveList::Item::DESKTOP_FOLDER:
            icon_id = SIID_DESKTOP;
            break;

        case proto::DriveList::Item::HOME_FOLDER:
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

CString GetDriveDisplayName(const proto::DriveList::Item& item)
{
    CString name;

    switch (item.type())
    {
        case proto::DriveList::Item::HOME_FOLDER:
            name.LoadStringW(IDS_FT_HOME_FOLDER);
            return name;

        case proto::DriveList::Item::DESKTOP_FOLDER:
            name.LoadStringW(IDS_FT_DESKTOP_FOLDER);
            return name;

        case proto::DriveList::Item::CDROM:
        case proto::DriveList::Item::FIXED:
        case proto::DriveList::Item::REMOVABLE:
        case proto::DriveList::Item::REMOTE:
        case proto::DriveList::Item::RAM:
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

CString GetDriveDescription(proto::DriveList::Item::Type type)
{
    CString desc;

    switch (type)
    {
        case proto::DriveList::Item::HOME_FOLDER:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_HOME);
            break;

        case proto::DriveList::Item::DESKTOP_FOLDER:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_DESKTOP);
            break;

        case proto::DriveList::Item::CDROM:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_CDROM);
            break;

        case proto::DriveList::Item::FIXED:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_FIXED);
            break;

        case proto::DriveList::Item::REMOVABLE:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_REMOVABLE);
            break;

        case proto::DriveList::Item::REMOTE:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_REMOTE);
            break;

        case proto::DriveList::Item::RAM:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_RAM);
            break;

        default:
            desc.LoadStringW(IDS_FT_DRIVE_DESC_UNKNOWN);
            break;
    }

    return desc;
}

CString GetDirectoryTypeString(const std::wstring& dir_name)
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

CString GetFileTypeString(const std::wstring& file_name)
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

    CString units;
    uint64_t divider;

    if (size >= kTB)
    {
        units.LoadStringW(IDS_FT_SIZE_TBYTES);
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units.LoadStringW(IDS_FT_SIZE_GBYTES);
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units.LoadStringW(IDS_FT_SIZE_MBYTES);
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units.LoadStringW(IDS_FT_SIZE_KBYTES);
        divider = kKB;
    }
    else
    {
        units.LoadStringW(IDS_FT_SIZE_BYTES);
        divider = 1;
    }

    return StringPrintfW(L"%.2f %s",
                         double(size) / double(divider),
                         units);
}

} // namespace aspia
