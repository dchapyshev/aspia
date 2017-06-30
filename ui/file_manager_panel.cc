//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_panel.h"
#include "ui/file_manager_helpers.h"
#include "ui/base/module.h"
#include "ui/resource.h"
#include "base/scoped_gdi_object.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"

#include <uxtheme.h>

namespace aspia {

static const int kNewFolderIndex = -1;
static const int kComputerIndex = -1;
static const int kCurrentFolderIndex = -2;

static int GetICLColor()
{
    DEVMODEW mode = { 0 };
    mode.dmSize = sizeof(mode);

    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode))
    {
        switch (mode.dmBitsPerPel)
        {
            case 32:
                return ILC_COLOR32;

            case 24:
                return ILC_COLOR24;

            case 16:
                return ILC_COLOR16;

            case 8:
                return ILC_COLOR8;

            case 4:
                return ILC_COLOR4;
        }
    }

    return ILC_COLOR32;
}

bool UiFileManagerPanel::CreatePanel(HWND parent,
                                     PanelType panel_type,
                                     Delegate* delegate)
{
    delegate_ = delegate;
    panel_type_ = panel_type;

    module_ = UiModule::Current();

    return Create(parent, WS_CHILD | WS_VISIBLE);
}

void UiFileManagerPanel::ReadDriveList(std::unique_ptr<proto::DriveList> drive_list)
{
    drive_combo_.DeleteAllItems();
    drive_imagelist_.RemoveAll();

    drive_list_.reset(drive_list.release());

    CIcon icon(GetComputerIcon());

    int icon_index = drive_imagelist_.AddIcon(icon);

    int root_index =
        drive_combo_.AddItem(module_.String(IDS_FT_COMPUTER),
                             icon_index, 0, kComputerIndex);

    const int count = drive_list_->item_size();

    for (int index = 0; index < count; ++index)
    {
        const proto::DriveListItem& item = drive_list_->item(index);

        icon = GetDriveIcon(item.type());
        icon_index = drive_imagelist_.AddIcon(icon);

        std::wstring display_name = GetDriveDisplayName(item);

        drive_combo_.AddItem(display_name, icon_index, 1, index);
    }

    if (!directory_list_)
    {
        drive_combo_.SelectItem(root_index);
        OnDriveChange();
    }
}

void UiFileManagerPanel::ReadDirectoryList(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    list_.DeleteAllItems();
    list_imagelist_.RemoveAll();
    SetFolderViews();

    directory_list_.reset(directory_list.release());

    // All directories have the same icon.
    ScopedHICON icon(GetDirectoryIcon());

    int current_folder_index = drive_combo_.GetItemWithData(kCurrentFolderIndex);
    if (current_folder_index != CB_ERR)
    {
        int icon_index = drive_combo_.GetItemImage(current_folder_index);
        if (icon_index != -1)
            drive_imagelist_.Remove(icon_index);

        drive_combo_.DeleteItem(current_folder_index);
    }

    int known_index = GetKnownDriveIndex(directory_list_->path());
    if (known_index == -1)
    {
        int icon_index = drive_imagelist_.AddIcon(icon);
        drive_combo_.InsertItem(UNICODEfromUTF8(directory_list_->path()),
                                 0, icon_index, 0, kCurrentFolderIndex);
        known_index = kCurrentFolderIndex;
    }

    drive_combo_.SelectItemData(known_index);

    int icon_index = list_imagelist_.AddIcon(icon);
    int count = directory_list_->item_size();

    // Enumerate the directories first.
    for (int index = 0; index < count; ++index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(index);

        if (item.type() != proto::DirectoryListItem::DIRECTORY)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        int item_index = list_.AddItem(list_.GetItemCount(), 0, name.c_str(), icon_index);
        list_.SetItemData(item_index, index);
        list_.SetItemText(item_index, 2, GetDirectoryTypeString(name).c_str());
        list_.SetItemText(item_index, 3, TimeToString(item.modified()).c_str());
    }

    // Enumerate the files.
    for (int index = 0; index < count; ++index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(index);

        if (item.type() != proto::DirectoryListItem::FILE)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        icon.Reset(GetFileIcon(name));
        icon_index = list_imagelist_.AddIcon(icon);

        int item_index = list_.AddItem(list_.GetItemCount(), 0, name.c_str(), icon_index);
        list_.SetItemData(item_index, index);
        list_.SetItemText(item_index, 1, SizeToString(item.size()).c_str());
        list_.SetItemText(item_index, 2, GetFileTypeString(name).c_str());
        list_.SetItemText(item_index, 3, TimeToString(item.modified()).c_str());
    }
}

int UiFileManagerPanel::GetColumnCount()
{
    CHeaderCtrl header(list_.GetHeader());

    if (!header)
        return 0;

    return header.GetItemCount();
}

void UiFileManagerPanel::DeleteAllColumns()
{
    int count = GetColumnCount();

    while (--count >= 0)
        list_.DeleteColumn(count);
}

void UiFileManagerPanel::AddColumn(UINT string_id, int width)
{
    CString text;
    text.LoadStringW(string_id);

    int column_index = list_.AddColumn(text, GetColumnCount());
    list_.SetColumnWidth(column_index, width);
}

void UiFileManagerPanel::SetComputerViews()
{
    DeleteAllColumns();

    AddColumn(IDS_FT_COLUMN_NAME, 130);
    AddColumn(IDS_FT_COLUMN_TYPE, 150);
    AddColumn(IDS_FT_COLUMN_TOTAL_SPACE, 80);
    AddColumn(IDS_FT_COLUMN_FREE_SPACE, 80);

    list_.ModifyStyle(LVS_EDITLABELS, LVS_SINGLESEL);

    toolbar_.EnableButton(ID_FOLDER_ADD, false);
    toolbar_.EnableButton(ID_FOLDER_UP, false);
    toolbar_.EnableButton(ID_DELETE, false);
    toolbar_.EnableButton(ID_SEND, false);
    toolbar_.EnableButton(ID_HOME, false);

    OnListItemChanged();
}

void UiFileManagerPanel::SetFolderViews()
{
    DeleteAllColumns();

    AddColumn(IDS_FT_COLUMN_NAME, 180);
    AddColumn(IDS_FT_COLUMN_SIZE, 70);
    AddColumn(IDS_FT_COLUMN_TYPE, 100);
    AddColumn(IDS_FT_COLUMN_MODIFIED, 100);

    list_.ModifyStyle(LVS_SINGLESEL, LVS_EDITLABELS);

    toolbar_.EnableButton(ID_FOLDER_ADD, true);
    toolbar_.EnableButton(ID_FOLDER_UP, true);
    toolbar_.EnableButton(ID_DELETE, true);
    toolbar_.EnableButton(ID_SEND, true);
    toolbar_.EnableButton(ID_HOME, true);

    OnListItemChanged();
}

void UiFileManagerPanel::AddToolBarIcon(UINT icon_id)
{
    CIcon icon(AtlLoadIconImage(icon_id,
                                LR_CREATEDIBSECTION,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON)));
    toolbar_imagelist_.AddIcon(icon);
}

void UiFileManagerPanel::OnCreate()
{
    HFONT default_font =
        reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    std::wstring panel_name;

    if (panel_type_ == PanelType::LOCAL)
        panel_name = module_.String(IDS_FT_LOCAL_COMPUTER);
    else
        panel_name = module_.String(IDS_FT_REMOTE_COMPUTER);

    CRect title_rect(0, 0, 200, 20);
    title_.Create(hwnd(), title_rect, panel_name.c_str(), WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    title_.SetFont(default_font);

    drive_combo_.Create(hwnd(),
                        IDC_ADDRESS_COMBO, WS_VSCROLL | CBS_DROPDOWN,
                        module_.Handle());
    drive_combo_.SetFont(default_font);

    drive_imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON),
                            ILC_MASK | GetICLColor(),
                            1, 1);
    drive_combo_.SetImageList(drive_imagelist_);

    toolbar_.Create(hwnd(), TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS);

    toolbar_.ModifyExtendedStyle(0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER);

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        { 0, ID_REFRESH,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 1, ID_DELETE,     TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 2, ID_FOLDER_ADD, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 3, ID_FOLDER_UP,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 4, ID_HOME,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, { 0 }, 0, -1 },
        { 5, ID_SEND,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT, { 0 }, 0, -1 }
    };

    toolbar_.ButtonStructSize(sizeof(kButtons[0]));
    toolbar_.AddButtons(_countof(kButtons), kButtons);

    toolbar_imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON),
                              ILC_MASK | GetICLColor(),
                              1, 1);
    toolbar_.SetImageList(toolbar_imagelist_);

    AddToolBarIcon(IDI_REFRESH);
    AddToolBarIcon(IDI_DELETE);
    AddToolBarIcon(IDI_FOLDER_ADD);
    AddToolBarIcon(IDI_FOLDER_UP);
    AddToolBarIcon(IDI_HOME);

    if (panel_type_ == PanelType::LOCAL)
    {
        AddToolBarIcon(IDI_SEND);
        toolbar_.SetButtonText(ID_SEND, module_.String(IDS_FT_SEND));
    }
    else
    {
        DCHECK(panel_type_ == PanelType::REMOTE);
        AddToolBarIcon(IDI_RECIEVE);
        toolbar_.SetButtonText(ID_SEND, module_.String(IDS_FT_RECIEVE));
    }

    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        LVS_REPORT | LVS_SHOWSELALWAYS;

    list_.Create(hwnd(), 0, 0, style, WS_EX_CLIENTEDGE);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(list_, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list_.SetExtendedListViewStyle(ex_style);
    list_imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                           GetSystemMetrics(SM_CYSMICON),
                           ILC_MASK | GetICLColor(),
                           1, 1);
    list_.SetImageList(list_imagelist_, LVSIL_SMALL);

    CRect status_rect(0, 0, 200, 20);
    status_.Create(hwnd(), status_rect, L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    status_.SetFont(default_font);

    delegate_->OnDriveListRequest(panel_type_);
}

void UiFileManagerPanel::OnDestroy()
{
    list_.DestroyWindow();
    toolbar_.DestroyWindow();
    title_.DestroyWindow();
    drive_combo_.DestroyWindow();
    status_.DestroyWindow();
}

void UiFileManagerPanel::OnSize(int width, int height)
{
    HDWP dwp = BeginDeferWindowPos(5);

    if (dwp)
    {
        toolbar_.AutoSize();

        CRect drive_rect;
        GetWindowRect(drive_combo_, drive_rect);

        CRect toolbar_rect;
        GetWindowRect(toolbar_, toolbar_rect);

        CRect title_rect;
        title_.GetWindowRect(title_rect);

        CRect status_rect;
        status_.GetWindowRect(status_rect);

        DeferWindowPos(dwp, title_, nullptr,
                       0,
                       0,
                       width,
                       title_rect.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, drive_combo_, nullptr,
                       0,
                       title_rect.Height(),
                       width,
                       drive_rect.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, toolbar_, nullptr,
                       0,
                       title_rect.Height() + drive_rect.Height(),
                       width,
                       toolbar_rect.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        int list_y = title_rect.Height() + toolbar_rect.Height() + drive_rect.Height();
        int list_height = height - list_y - status_rect.Height();

        DeferWindowPos(dwp, list_, nullptr,
                       0,
                       list_y,
                       width,
                       list_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, status_, nullptr,
                       0,
                       list_y + list_height,
                       width,
                       status_rect.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }
}

void UiFileManagerPanel::OnDrawItem(LPDRAWITEMSTRUCT dis)
{
    if (dis->hwndItem == title_ || dis->hwndItem == status_)
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
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(phdr);

    switch (header->hdr.idFrom)
    {
        case ID_REFRESH:
            header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_REFRESH);
            break;

        case ID_DELETE:
            header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_DELETE);
            break;

        case ID_FOLDER_ADD:
            header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_FOLDER_ADD);
            break;

        case ID_FOLDER_UP:
            header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_FOLDER_UP);
            break;

        case ID_HOME:
            header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_HOME);
            break;

        case ID_SEND:
        {
            if (panel_type_ == PanelType::LOCAL)
                header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_SEND);
            else
                header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_RECIEVE);
        }
        break;

        default:
            return;
    }

    header->hinst = module_.Handle();
}

int UiFileManagerPanel::GetKnownDriveIndex(const std::string& path)
{
    if (drive_list_)
    {
        const int count = drive_list_->item_size();

        for (int index = 0; index < count; ++index)
        {
            if (_stricmp(path.c_str(), drive_list_->item(index).path().c_str()) == 0)
                return index;
        }
    }

    return -1;
}

void UiFileManagerPanel::OnDriveChange()
{
    if (!drive_list_)
        return;

    int selected_item = drive_combo_.GetSelectedItem();
    if (selected_item == CB_ERR)
        return;

    int object_index = drive_combo_.GetItemData(selected_item);

    if (object_index == kComputerIndex)
    {
        list_.DeleteAllItems();
        list_imagelist_.RemoveAll();
        directory_list_.reset();

        SetComputerViews();

        const int count = drive_list_->item_size();

        for (int index = 0; index < count; ++index)
        {
            const proto::DriveListItem& item = drive_list_->item(index);

            ScopedHICON icon(GetDriveIcon(item.type()));
            int icon_index = list_imagelist_.AddIcon(icon);

            std::wstring display_name = GetDriveDisplayName(item);

            int item_index = list_.AddItem(list_.GetItemCount(), 0, display_name.c_str(), icon_index);
            list_.SetItemData(item_index, index);
            list_.SetItemText(item_index, 1, GetDriveDescription(item.type()).c_str());

            if (item.total_space())
                list_.SetItemText(item_index, 2, SizeToString(item.total_space()).c_str());

            if (item.free_space())
                list_.SetItemText(item_index, 3, SizeToString(item.free_space()).c_str());
        }
    }
    else if (object_index == kCurrentFolderIndex)
    {
        std::wstring path = drive_combo_.GetItemText(selected_item);
        delegate_->OnDirectoryListRequest(panel_type_,
                                          UTF8fromUNICODE(path),
                                          std::string());
    }
    else if (object_index < 0 || object_index >= drive_list_->item_size())
    {
        return;
    }
    else
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          drive_list_->item(object_index).path(),
                                          std::string());
    }
}

int UiFileManagerPanel::GetItemUnderMousePointer()
{
    LVHITTESTINFO hti;
    memset(&hti, 0, sizeof(hti));

    if (GetCursorPos(&hti.pt))
    {
        if (list_.ScreenToClient(&hti.pt) != FALSE)
        {
            hti.flags = LVHT_ONITEMICON | LVHT_ONITEMLABEL;
            return list_.HitTest(&hti);
        }
    }

    return -1;
}

void UiFileManagerPanel::OnDirectoryChange()
{
    int item_index = GetItemUnderMousePointer();
    if (item_index == -1)
        return;

    int object_index = list_.GetItemData(item_index);

    if (!directory_list_)
    {
        if (!drive_list_)
            return;

        drive_combo_.SelectItemData(object_index);
        OnDriveChange();

        return;
    }

    if (object_index < 0 || object_index >= directory_list_->item_size())
        return;

    const proto::DirectoryListItem& item =
        directory_list_->item(object_index);

    if (item.type() == proto::DirectoryListItem::DIRECTORY)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          item.name());
    }
}

void UiFileManagerPanel::OnFolderUp()
{
    if (!directory_list_ || !directory_list_->has_parent())
    {
        OnMoveToComputer();
    }
    else
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          "..");
    }
}

void UiFileManagerPanel::OnFolderCreate()
{
    if (!directory_list_)
        return;

    ScopedHICON folder_icon(GetDirectoryIcon());
    int icon_index = list_imagelist_.AddIcon(folder_icon);

    std::wstring folder_name = module_.String(IDS_FT_NEW_FOLDER);

    SetFocus(list_);

    int item_index =
        list_.AddItem(list_.GetItemCount(), 0, folder_name.c_str(), icon_index);
    list_.SetItemData(item_index, kNewFolderIndex);

    list_.SetItemText(item_index, 2, GetDirectoryTypeString(folder_name).c_str());
    list_.EditLabel(item_index);
}

void UiFileManagerPanel::OnRefresh()
{
    delegate_->OnDriveListRequest(panel_type_);

    if (directory_list_)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          std::string());
    }
}

void UiFileManagerPanel::OnRemove()
{
    if (!directory_list_)
        return;

    UINT selected_count = list_.GetSelectedCount();
    if (!selected_count)
        return;

    std::wstring title = module_.String(IDS_CONFIRMATION);
    std::wstring object_list;
    std::wstring format;

    if (selected_count == 1)
    {
        int selected_item = list_.GetNextItem(-1, LVNI_SELECTED);
        if (selected_item == -1)
            return;

        int object_index = list_.GetItemData(selected_item);
        if (object_index < 0 || object_index >= directory_list_->item_size())
            return;

        const proto::DirectoryListItem& item =
            directory_list_->item(object_index);

        if (item.type() == proto::DirectoryListItem::DIRECTORY)
            format = module_.String(IDS_FT_DELETE_CONFORM_DIR);
        else
            format = module_.String(IDS_FT_DELETE_CONFORM_FILE);

        object_list = UNICODEfromUTF8(item.name());
    }
    else
    {
        format = module_.String(IDS_FT_DELETE_CONFORM_MULTI);

        for (int item_index = list_.GetNextItem(-1, LVNI_SELECTED);
             item_index != -1;
             item_index = list_.GetNextItem(item_index, LVNI_SELECTED))
        {
            int object_index = list_.GetItemData(item_index);
            if (object_index < 0 || object_index >= directory_list_->item_size())
                continue;

            const proto::DirectoryListItem& item =
                directory_list_->item(object_index);

            object_list.append(UNICODEfromUTF8(item.name()));
            object_list.append(L"\r\n");
        }
    }

    std::wstring message = StringPrintfW(format.c_str(), object_list.c_str());

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        for (int item_index = list_.GetNextItem(-1, LVNI_SELECTED);
             item_index != -1;
             item_index = list_.GetNextItem(item_index, LVNI_SELECTED))
        {
            int object_index = list_.GetItemData(item_index);
            if (object_index < 0 || object_index >= directory_list_->item_size())
                continue;

            const proto::DirectoryListItem& item =
                directory_list_->item(object_index);

            delegate_->OnRemoveRequest(panel_type_,
                                       directory_list_->path(),
                                       item.name());
        }

        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          std::string());
    }
}

void UiFileManagerPanel::OnMoveToComputer()
{
    drive_combo_.SelectItemData(kComputerIndex);
    OnDriveChange();
}

void UiFileManagerPanel::OnEndLabelEdit(LPNMLVDISPINFOW disp_info)
{
    if (!directory_list_)
        return;

    int index = disp_info->item.lParam;

    // New folder.
    if (index == kNewFolderIndex)
    {
        CEdit edit(list_.GetEditControl());

        WCHAR buffer[MAX_PATH] = { 0 };
        edit.GetWindowTextW(buffer, _countof(buffer));

        delegate_->OnCreateDirectoryRequest(panel_type_,
                                            directory_list_->path(),
                                            UTF8fromUNICODE(buffer));
    }
    else // Rename exists item.
    {
        DCHECK(index >= 0 || index < directory_list_->item_size());

        // User canceled rename.
        if (!disp_info->item.pszText)
            return;

        delegate_->OnRenameRequest(panel_type_,
                                   directory_list_->path(),
                                   directory_list_->item(index).name(),
                                   UTF8fromUNICODE(disp_info->item.pszText));
    }

    delegate_->OnDirectoryListRequest(panel_type_,
                                      directory_list_->path(),
                                      std::string());
}

void UiFileManagerPanel::OnListItemChanged()
{
    UINT count = list_.GetSelectedCount();

    if (directory_list_)
    {
        bool enable = (count != 0);

        toolbar_.EnableButton(ID_DELETE, enable);
        toolbar_.EnableButton(ID_SEND, enable);
    }

    std::wstring format = module_.String(IDS_FT_SELECTED_OBJECT_COUNT);
    std::wstring status = StringPrintfW(format.c_str(), count);

    status_.SetWindowTextW(status.c_str());
}

void UiFileManagerPanel::OnSend()
{
    if (!directory_list_)
        return;

    UINT selected_count = list_.GetSelectedCount();
    if (!selected_count)
        return;

    if (selected_count == 1)
    {
        int selected_item = list_.GetNextItem(-1, LVNI_SELECTED);
        if (selected_item == -1)
            return;

        int object_index = list_.GetItemData(selected_item);
        if (object_index < 0 || object_index >= directory_list_->item_size())
            return;

        const proto::DirectoryListItem& item =
            directory_list_->item(object_index);

        if (item.type() == proto::DirectoryListItem::DIRECTORY)
        {

        }
        else
        {

        }
    }
    else
    {

    }
}

bool UiFileManagerPanel::OnMessage(UINT msg,
                                   WPARAM wparam,
                                   LPARAM lparam,
                                   LRESULT* result)
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

            if (header->code == TTN_GETDISPINFOW)
            {
                OnGetDispInfo(header);
            }
            else if (header->code == CBEN_ENDEDITW && header->hwndFrom == drive_combo_.hwnd())
            {
                PNMCBEENDEDITW end_edit = reinterpret_cast<PNMCBEENDEDITW>(lparam);

                if (end_edit->fChanged && end_edit->iWhy == CBENF_RETURN && end_edit->szText)
                {
                    delegate_->OnDirectoryListRequest(panel_type_,
                                                      UTF8fromUNICODE(end_edit->szText),
                                                      std::string());
                }
            }
            else if (header->hwndFrom == list_)
            {
                switch (header->code)
                {
                    case NM_DBLCLK:
                        OnDirectoryChange();
                        break;

                    case NM_RCLICK:
                        break;

                    case NM_CLICK:
                        break;

                    case LVN_ENDLABELEDIT:
                        OnEndLabelEdit(reinterpret_cast<LPNMLVDISPINFOW>(lparam));
                        break;

                    case LVN_ITEMCHANGED:
                        OnListItemChanged();
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
                            OnDriveChange();
                            break;
                    }
                }
                break;

                case ID_FOLDER_UP:
                    OnFolderUp();
                    break;

                case ID_FOLDER_ADD:
                    OnFolderCreate();
                    break;

                case ID_REFRESH:
                    OnRefresh();
                    break;

                case ID_DELETE:
                    OnRemove();
                    break;

                case ID_HOME:
                    OnMoveToComputer();
                    break;

                case ID_SEND:
                    OnSend();
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
