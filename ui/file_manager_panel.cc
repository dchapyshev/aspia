//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_panel.h"
#include "ui/base/module.h"
#include "ui/resource.h"
#include "ui/get_stock_icon.h"
#include "base/scoped_gdi_object.h"
#include "base/string_util.h"
#include "base/unicode.h"

#include <filesystem>
#include <shellapi.h>
#include <shlobj.h>

namespace aspia {

bool UiFileManagerPanel::CreatePanel(HWND parent, Type type, Delegate* delegate)
{
    delegate_ = delegate;
    type_ = type;

    const UiModule& module = UiModule::Current();

    if (type == Type::LOCAL)
    {
        name_ = module.string(IDS_FT_LOCAL_COMPUTER);
    }
    else
    {
        DCHECK(type == Type::REMOTE);
        name_ = module.string(IDS_FT_REMOTE_COMPUTER);
    }

    return Create(parent, WS_CHILD | WS_VISIBLE);
}

static HICON GetDriveIcon(proto::DriveListItem::Type drive_type)
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

static std::wstring GetDriveDisplayName(const proto::DriveListItem& item)
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

void UiFileManagerPanel::ReadDriveList(std::unique_ptr<proto::DriveList> drive_list)
{
    address_window_.DeleteAllItems();
    address_imagelist_.RemoveAll();

    drive_list_.reset(drive_list.release());

    const int count = drive_list_->item_size();

    for (int index = 0; index < count; ++index)
    {
        const proto::DriveListItem& item = drive_list_->item(index);

        ScopedHICON icon(GetDriveIcon(item.type()));

        int icon_index = -1;

        if (icon.IsValid())
            icon_index = address_imagelist_.AddIcon(icon);

        std::wstring display_name = GetDriveDisplayName(item);

        address_window_.AddItem(display_name, icon_index, 0, index);
    }
}

static HICON GetDirectoryIcon()
{
    SHSTOCKICONINFO icon_info;

    memset(&icon_info, 0, sizeof(icon_info));
    icon_info.cbSize = sizeof(icon_info);

    if (FAILED(GetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &icon_info)))
        return nullptr;

    return icon_info.hIcon;
}

static HICON GetFileIcon(const std::wstring& file_name)
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

static std::wstring SizeToString(uint64_t size)
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

static std::wstring TimeToString(time_t time)
{
    tm* local_time = std::localtime(&time);

    if (!local_time)
        return std::wstring();

    // Set the locale obtained from system.
    _wsetlocale(LC_TIME, L"");

    wchar_t string[128];
    if (!wcsftime(string, _countof(string), L"%x %X", local_time))
        return std::wstring();

    return string;
}

void UiFileManagerPanel::ReadDirectoryList(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    list_window_.DeleteAllItems();
    list_imagelist_.RemoveAll();

    directory_list_.reset(directory_list.release());

    const int count = directory_list_->item_size();

    int icon_index = -1;

    // All directories have the same icon.
    ScopedHICON icon(GetDirectoryIcon());

    if (icon.IsValid())
        icon_index = list_imagelist_.AddIcon(icon);

    // Enumerate the directories first.
    for (int index = 0; index < count; ++index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(index);

        if (item.type() != proto::DirectoryListItem::DIRECTORY)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        int item_index = list_window_.AddItem(name, index, icon_index);

        SHFILEINFO file_info;
        memset(&file_info, 0, sizeof(file_info));

        SHGetFileInfoW(name.c_str(),
                       FILE_ATTRIBUTE_DIRECTORY,
                       &file_info,
                       sizeof(file_info),
                       SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME);

        list_window_.SetItemText(item_index, 2, file_info.szTypeName);
        list_window_.SetItemText(item_index, 3, TimeToString(item.modified()));
    }

    // Enumerate the files.
    for (int index = 0; index < count; ++index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(index);

        if (item.type() != proto::DirectoryListItem::FILE)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        icon.Reset(GetFileIcon(name));

        icon_index = -1;

        if (icon.IsValid())
            icon_index = list_imagelist_.AddIcon(icon);

        int item_index = list_window_.AddItem(name, index, icon_index);

        list_window_.SetItemText(item_index, 1, SizeToString(item.size()));

        SHFILEINFO file_info;
        memset(&file_info, 0, sizeof(file_info));

        SHGetFileInfoW(name.c_str(),
                       FILE_ATTRIBUTE_NORMAL,
                       &file_info,
                       sizeof(file_info),
                       SHGFI_USEFILEATTRIBUTES | SHGFI_TYPENAME);

        list_window_.SetItemText(item_index, 2, file_info.szTypeName);
        list_window_.SetItemText(item_index, 3, TimeToString(item.modified()));
    }
}

void UiFileManagerPanel::OnCreate()
{
    const UiModule& module = UiModule().Current();

    HFONT default_font =
        reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    title_window_.Attach(CreateWindowExW(0,
                                         WC_STATICW,
                                         name_.c_str(),
                                         WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                         0, 0, 200, 20,
                                         hwnd(),
                                         nullptr,
                                         module.Handle(),
                                         nullptr));
    title_window_.SetFont(default_font);

    address_window_.Attach(CreateWindowExW(0,
                                           WC_COMBOBOXEXW,
                                           L"",
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                               WS_VSCROLL | CBS_DROPDOWN,
                                           0, 0, 200, 200,
                                           hwnd(),
                                           reinterpret_cast<HMENU>(IDC_ADDRESS_COMBO),
                                           module.Handle(),
                                           nullptr));
    address_window_.SetFont(default_font);

    if (address_imagelist_.CreateSmall())
    {
        address_window_.SetImageList(address_imagelist_);
    }

    toolbar_window_.Attach(CreateWindowExW(0,
                                           TOOLBARCLASSNAMEW,
                                           nullptr,
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBSTYLE_FLAT |
                                               TBSTYLE_TOOLTIPS,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           hwnd(),
                                           nullptr,
                                           module.Handle(),
                                           nullptr));

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        { 0, ID_REFRESH,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 1, ID_DELETE,     TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 2, ID_FOLDER_ADD, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 3, ID_FOLDER_UP,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 4, ID_HOME,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 5, ID_SEND,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 }
    };

    SendMessageW(toolbar_window_, TB_BUTTONSTRUCTSIZE, sizeof(kButtons[0]), 0);

    SendMessageW(toolbar_window_,
                 TB_ADDBUTTONS,
                 _countof(kButtons),
                 reinterpret_cast<LPARAM>(kButtons));

    if (toolbar_imagelist_.CreateSmall())
    {
        toolbar_imagelist_.AddIcon(module, IDI_REFRESH);
        toolbar_imagelist_.AddIcon(module, IDI_DELETE);
        toolbar_imagelist_.AddIcon(module, IDI_FOLDER_ADD);
        toolbar_imagelist_.AddIcon(module, IDI_FOLDER_UP);
        toolbar_imagelist_.AddIcon(module, IDI_HOME);

        if (type_ == Type::LOCAL)
        {
            toolbar_imagelist_.AddIcon(module, IDI_SEND);
        }
        else
        {
            DCHECK(type_ == Type::REMOTE);
            toolbar_imagelist_.AddIcon(module, IDI_RECIEVE);
        }

        SendMessageW(toolbar_window_,
                     TB_SETIMAGELIST,
                     0,
                     reinterpret_cast<LPARAM>(toolbar_imagelist_.Handle()));
    }

    list_window_.Create(hwnd(),
                        WS_EX_CLIENTEDGE,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
                        module.Handle());

    list_window_.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);

    list_window_.AddColumn(module.string(IDS_FT_COLUMN_NAME), 180);
    list_window_.AddColumn(module.string(IDS_FT_COLUMN_SIZE), 60);
    list_window_.AddColumn(module.string(IDS_FT_COLUMN_TYPE), 100);
    list_window_.AddColumn(module.string(IDS_FT_COLUMN_MODIFIED), 100);

    status_window_.Attach(CreateWindowExW(0,
                                          WC_STATICW,
                                          L"",
                                          WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                          0, 0, 200, 20,
                                          hwnd(),
                                          nullptr,
                                          module.Handle(),
                                          nullptr));
    status_window_.SetFont(default_font);

    if (list_imagelist_.CreateSmall())
    {
        list_window_.SetImageList(list_imagelist_, LVSIL_SMALL);
    }
}

void UiFileManagerPanel::OnDestroy()
{
    list_window_.DestroyWindow();
    toolbar_window_.DestroyWindow();
    title_window_.DestroyWindow();
    address_window_.DestroyWindow();
    status_window_.DestroyWindow();
}

void UiFileManagerPanel::OnSize(int width, int height)
{
    HDWP dwp = BeginDeferWindowPos(5);

    if (dwp)
    {
        SendMessageW(toolbar_window_, TB_AUTOSIZE, 0, 0);

        int address_height = address_window_.Height();
        int toolbar_height = toolbar_window_.Height();
        int title_height = title_window_.Height();
        int status_height = status_window_.Height();

        DeferWindowPos(dwp, title_window_, nullptr,
                       0,
                       0,
                       width,
                       title_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, address_window_, nullptr,
                       0,
                       title_height,
                       width,
                       address_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, toolbar_window_, nullptr,
                       0,
                       title_height + address_height,
                       width,
                       toolbar_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        int list_y = title_height + toolbar_height + address_height;
        int list_height = height - list_y - status_height;

        DeferWindowPos(dwp, list_window_, nullptr,
                       0,
                       list_y,
                       width,
                       list_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, status_window_, nullptr,
                       0,
                       list_y + list_height,
                       width,
                       status_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }
}

void UiFileManagerPanel::OnDrawItem(LPDRAWITEMSTRUCT dis)
{
    if (dis->hwndItem == title_window_.hwnd() ||
        dis->hwndItem == status_window_.hwnd())
    {
        int saved_dc = SaveDC(dis->hDC);

        if (saved_dc)
        {
            // Transparent background.
            SetBkMode(dis->hDC, TRANSPARENT);

            HBRUSH background_brush = GetSysColorBrush(COLOR_WINDOW);
            FillRect(dis->hDC, &dis->rcItem, background_brush);

            WCHAR label[256] = { 0 };
            GetWindowTextW(dis->hwndItem, label, _countof(label));

            if (label[0])
            {
                SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));

                DrawTextW(dis->hDC,
                          label,
                          wcslen(label),
                          &dis->rcItem,
                          DT_VCENTER | DT_SINGLELINE);
            }

            RestoreDC(dis->hDC, saved_dc);
        }
    }
}

void UiFileManagerPanel::OnGetDispInfo(LPNMHDR phdr)
{
    LPTOOLTIPTEXTW header = reinterpret_cast<LPTOOLTIPTEXTW>(phdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_REFRESH:
            string_id = IDS_FT_TOOLTIP_REFRESH;
            break;

        case ID_DELETE:
            string_id = IDS_FT_TOOLTIP_DELETE;
            break;

        case ID_FOLDER_ADD:
            string_id = IDS_FT_TOOLTIP_FOLDER_ADD;
            break;

        case ID_FOLDER_UP:
            string_id = IDS_FT_TOOLTIP_FOLDER_UP;
            break;

        case ID_HOME:
            string_id = IDS_FT_TOOLTIP_HOME;
            break;

        case ID_SEND:
        {
            if (type_ == Type::LOCAL)
                string_id = IDS_FT_TOOLTIP_SEND;
            else
                string_id = IDS_FT_TOOLTIP_RECIEVE;
        }
        break;

        default:
            return;
    }

    LoadStringW(UiModule().Current().Handle(),
                string_id,
                header->szText,
                _countof(header->szText));
}

void UiFileManagerPanel::OnAddressChanged()
{
    if (!drive_list_)
        return;

    int selected_item = address_window_.GetSelectedItem();
    if (selected_item == CB_ERR)
        return;

    int index = address_window_.GetItemData(selected_item);
    if (index < 0 || index >= drive_list_->item_size())
        return;

    delegate_->OnDirectoryListRequest(type_, drive_list_->item(index).path());
}

void UiFileManagerPanel::OnAddressChange()
{
    if (!directory_list_)
        return;

    int item_index = list_window_.GetItemUnderPointer();
    if (item_index == -1)
        return;

    int index = list_window_.GetItemData<int>(item_index);
    if (index < 0 || index >= directory_list_->item_size())
        return;

    const proto::DirectoryListItem& item = directory_list_->item(index);

    if (item.type() == proto::DirectoryListItem::DIRECTORY)
    {
        std::experimental::filesystem::path path =
            std::experimental::filesystem::u8path(directory_list_->path());

        path.append(item.name());

        delegate_->OnDirectoryListRequest(type_, path.u8string());
    }
}

void UiFileManagerPanel::OnFolderUp()
{
    if (!directory_list_)
        return;

    std::experimental::filesystem::path path =
        std::experimental::filesystem::u8path(directory_list_->path());

    if (!path.has_parent_path())
        return;

    path = path.parent_path();

    if (path.has_root_name() && path.root_name() == path)
        return;

    delegate_->OnDirectoryListRequest(type_, path.u8string());
}

bool UiFileManagerPanel::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    switch (msg)
    {
        case WM_CREATE:
            OnCreate();
            break;

        case WM_DESTROY:
            OnDestroy();
            break;

        case WM_DRAWITEM:
            OnDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lparam));
            break;

        case WM_SIZE:
            OnSize(LOWORD(lparam), HIWORD(lparam));
            break;

        case WM_NOTIFY:
        {
            LPNMHDR header = reinterpret_cast<LPNMHDR>(lparam);

            if (header->hwndFrom == toolbar_window_.hwnd())
            {
                switch (header->code)
                {
                    case TTN_GETDISPINFO:
                        OnGetDispInfo(header);
                        break;
                }
            }
            else if (header->hwndFrom == list_window_.hwnd())
            {
                switch (header->code)
                {
                    case NM_DBLCLK:
                        OnAddressChange();
                        break;

                    case NM_RCLICK:
                        break;

                    case NM_CLICK:
                        break;
                }
            }
        }
        break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDC_ADDRESS_COMBO:
                {
                    switch (HIWORD(wparam))
                    {
                        case CBN_SELCHANGE:
                            OnAddressChanged();
                            break;
                    }
                }
                break;

                case ID_FOLDER_UP:
                    OnFolderUp();
                    break;
            }
        }
        break;

        default:
            return false;
    }

    *result = 0;
    return true;
}

} // namespace aspia
