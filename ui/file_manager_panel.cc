//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_panel.h"
#include "ui/file_manager_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/logging.h"

#include <uxtheme.h>

namespace aspia {

static const int kNewFolderObjectIndex = -1;
static const int kComputerObjectIndex = -1;
static const int kCurrentFolderObjectIndex = -2;

UiFileManagerPanel::UiFileManagerPanel(PanelType panel_type,
                                       Delegate* delegate) :
    panel_type_(panel_type),
    delegate_(delegate)
{
    // Nothing
}

bool UiFileManagerPanel::IsValidDirectoryObjectIndex(int object_index)
{
    if (object_index >= 0 && object_index < directory_list_->item_size())
        return true;

    return false;
}

void UiFileManagerPanel::ReadDriveList(std::unique_ptr<proto::DriveList> drive_list)
{
    drive_combo_.ResetContent();
    drive_imagelist_.RemoveAll();

    drive_list_.reset(drive_list.release());

    CIcon icon(GetComputerIcon());

    int icon_index = drive_imagelist_.AddIcon(icon);

    CString text;
    text.LoadStringW(IDS_FT_COMPUTER);

    drive_combo_.AddItem(text, icon_index, icon_index, 0, kComputerObjectIndex);

    const int object_count = drive_list_->item_size();

    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DriveListItem& item = drive_list_->item(object_index);

        icon = GetDriveIcon(item.type());
        icon_index = drive_imagelist_.AddIcon(icon);

        drive_combo_.AddItem(GetDriveDisplayName(item),
                             icon_index,
                             icon_index,
                             1,
                             object_index);
    }

    if (!directory_list_)
    {
        MoveToDrive(kComputerObjectIndex);
    }
}

int UiFileManagerPanel::GetDriveIndexByObjectIndex(int object_index)
{
    DWORD_PTR data = static_cast<DWORD_PTR>(object_index);
    int item_count = drive_combo_.GetCount();

    for (int item_index = 0; item_index < item_count; ++item_index)
    {
        if (drive_combo_.GetItemData(item_index) == data)
            return item_index;
    }

    return CB_ERR;
}

int UiFileManagerPanel::SelectDriveByObjectIndex(int object_index)
{
    int item_index = GetDriveIndexByObjectIndex(object_index);

    if (item_index != CB_ERR)
        drive_combo_.SetCurSel(item_index);

    return item_index;
}

void UiFileManagerPanel::ReadDirectoryList(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    list_.DeleteAllItems();
    list_imagelist_.RemoveAll();
    SetFolderViews();

    directory_list_.reset(directory_list.release());

    // All directories have the same icon.
    CIcon icon(GetDirectoryIcon());

    int current_folder_index = GetDriveIndexByObjectIndex(kCurrentFolderObjectIndex);
    if (current_folder_index != CB_ERR)
    {
        COMBOBOXEXITEMW item;
        memset(&item, 0, sizeof(item));

        item.mask = CBEIF_IMAGE;
        item.iItem = current_folder_index;

        if (drive_combo_.GetItem(&item))
            drive_imagelist_.Remove(item.iImage);

        drive_combo_.DeleteItem(current_folder_index);
    }

    int known_object_index = GetKnownDriveObjectIndex(directory_list_->path());
    if (known_object_index == -1)
    {
        known_object_index = kCurrentFolderObjectIndex;

        int icon_index = drive_imagelist_.AddIcon(icon);

        drive_combo_.InsertItem(0,
                                UNICODEfromUTF8(directory_list_->path()).c_str(),
                                icon_index,
                                icon_index,
                                0,
                                known_object_index);
    }

    SelectDriveByObjectIndex(known_object_index);

    int icon_index = list_imagelist_.AddIcon(icon);
    int object_count = directory_list_->item_size();

    // Enumerate the directories first.
    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(object_index);

        if (item.type() != proto::DirectoryListItem::DIRECTORY)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        int item_index = list_.AddItem(list_.GetItemCount(), 0, name.c_str(), icon_index);
        list_.SetItemData(item_index, object_index);
        list_.SetItemText(item_index, 2, GetDirectoryTypeString(name));
        list_.SetItemText(item_index, 3, TimeToString(item.modified()).c_str());
    }

    // Enumerate the files.
    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DirectoryListItem& item = directory_list_->item(object_index);

        if (item.type() != proto::DirectoryListItem::FILE)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        icon = GetFileIcon(name);
        icon_index = list_imagelist_.AddIcon(icon);

        int item_index = list_.AddItem(list_.GetItemCount(), 0, name.c_str(), icon_index);
        list_.SetItemData(item_index, object_index);
        list_.SetItemText(item_index, 1, SizeToString(item.size()).c_str());
        list_.SetItemText(item_index, 2, GetFileTypeString(name));
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

    toolbar_.EnableButton(ID_FOLDER_ADD, FALSE);
    toolbar_.EnableButton(ID_FOLDER_UP, FALSE);
    toolbar_.EnableButton(ID_DELETE, FALSE);
    toolbar_.EnableButton(ID_SEND, FALSE);
    toolbar_.EnableButton(ID_HOME, FALSE);
}

void UiFileManagerPanel::SetFolderViews()
{
    DeleteAllColumns();

    AddColumn(IDS_FT_COLUMN_NAME, 180);
    AddColumn(IDS_FT_COLUMN_SIZE, 70);
    AddColumn(IDS_FT_COLUMN_TYPE, 100);
    AddColumn(IDS_FT_COLUMN_MODIFIED, 100);

    list_.ModifyStyle(LVS_SINGLESEL, LVS_EDITLABELS);

    toolbar_.EnableButton(ID_FOLDER_ADD, TRUE);
    toolbar_.EnableButton(ID_FOLDER_UP, TRUE);
    toolbar_.EnableButton(ID_DELETE, TRUE);
    toolbar_.EnableButton(ID_SEND, TRUE);
    toolbar_.EnableButton(ID_HOME, TRUE);
}

void UiFileManagerPanel::AddToolBarIcon(UINT icon_id, const CSize& icon_size)
{
    CIcon icon(AtlLoadIconImage(icon_id,
                                LR_CREATEDIBSECTION,
                                icon_size.cx,
                                icon_size.cy));
    toolbar_imagelist_.AddIcon(icon);
}

void UiFileManagerPanel::SetToolBarButtonText(int command_id, UINT resource_id)
{
    int button_index = toolbar_.CommandToIndex(command_id);
    if (button_index == -1)
        return;

    TBBUTTON button = { 0 };

    if (!toolbar_.GetButton(button_index, &button))
        return;

    CString string;
    string.LoadStringW(resource_id);

    int string_id = toolbar_.AddStrings(string);
    if (string_id == -1)
        return;

    button.iString = string_id;

    toolbar_.DeleteButton(button_index);
    toolbar_.InsertButton(button_index, &button);
}

LPARAM UiFileManagerPanel::OnCreate(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    HFONT default_font = AtlGetStockFont(DEFAULT_GUI_FONT);

    CSize small_icon_size(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON));

    CString panel_name;

    if (panel_type_ == PanelType::LOCAL)
        panel_name.LoadStringW(IDS_FT_LOCAL_COMPUTER);
    else
        panel_name.LoadStringW(IDS_FT_REMOTE_COMPUTER);

    CRect title_rect(0, 0, 200, 20);
    title_.Create(*this, title_rect, panel_name, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    title_.SetFont(default_font);

    CRect drive_rect(0, 0, 200, 200);
    drive_combo_.Create(*this, drive_rect, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN,
                        0, kDriveControl);
    drive_combo_.SetFont(default_font);

    if (drive_imagelist_.Create(small_icon_size.cx,
                                small_icon_size.cy,
                                ILC_MASK | ILC_COLOR32,
                                1, 1))
    {
        drive_combo_.SetImageList(drive_imagelist_);
    }

    toolbar_.Create(*this, CWindow::rcDefault, nullptr,
                    WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT |
                        TBSTYLE_LIST | TBSTYLE_TOOLTIPS,
                    0, kToolBarControl);

    toolbar_.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER);

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

    toolbar_.SetButtonStructSize(sizeof(kButtons[0]));
    toolbar_.AddButtons(_countof(kButtons), kButtons);

    if (toolbar_imagelist_.Create(small_icon_size.cx,
                                  small_icon_size.cy,
                                  ILC_MASK | ILC_COLOR32,
                                  1, 1))
    {
        toolbar_.SetImageList(toolbar_imagelist_);

        AddToolBarIcon(IDI_REFRESH, small_icon_size);
        AddToolBarIcon(IDI_DELETE, small_icon_size);
        AddToolBarIcon(IDI_FOLDER_ADD, small_icon_size);
        AddToolBarIcon(IDI_FOLDER_UP, small_icon_size);
        AddToolBarIcon(IDI_HOME, small_icon_size);

        if (panel_type_ == PanelType::LOCAL)
        {
            AddToolBarIcon(IDI_SEND, small_icon_size);
            SetToolBarButtonText(ID_SEND, IDS_FT_SEND);
        }
        else
        {
            DCHECK(panel_type_ == PanelType::REMOTE);
            AddToolBarIcon(IDI_RECIEVE, small_icon_size);
            SetToolBarButtonText(ID_SEND, IDS_FT_RECIEVE);
        }
    }

    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        LVS_REPORT | LVS_SHOWSELALWAYS;

    list_.Create(*this, 0, 0, style, WS_EX_CLIENTEDGE, kListControl);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(list_, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list_.SetExtendedListViewStyle(ex_style);

    if (list_imagelist_.Create(small_icon_size.cx,
                               small_icon_size.cy,
                               ILC_MASK | ILC_COLOR32,
                               1, 1))
    {
        list_.SetImageList(list_imagelist_, LVSIL_SMALL);
    }

    CRect status_rect(0, 0, 200, 20);
    status_.Create(*this, status_rect, nullptr,
                   WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    status_.SetFont(default_font);

    delegate_->OnDriveListRequest(panel_type_);
    return 0;
}

LRESULT UiFileManagerPanel::OnDestroy(UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL& handled)
{
    list_.DestroyWindow();
    toolbar_.DestroyWindow();
    title_.DestroyWindow();
    drive_combo_.DestroyWindow();
    status_.DestroyWindow();
    return 0;
}

LRESULT UiFileManagerPanel::OnSize(UINT message,
                                   WPARAM wparam,
                                   LPARAM lparam,
                                   BOOL& handled)
{
    HDWP dwp = BeginDeferWindowPos(5);

    if (dwp)
    {
        CSize size(lparam);

        toolbar_.AutoSize();

        CRect drive_rect;
        drive_combo_.GetWindowRect(drive_rect);

        CRect toolbar_rect;
        toolbar_.GetWindowRect(toolbar_rect);

        CRect title_rect;
        title_.GetWindowRect(title_rect);

        CRect status_rect;
        status_.GetWindowRect(status_rect);

        title_.DeferWindowPos(dwp, nullptr,
                              0,
                              0,
                              size.cx,
                              title_rect.Height(),
                              SWP_NOACTIVATE | SWP_NOZORDER);

        drive_combo_.DeferWindowPos(dwp, nullptr,
                                    0,
                                    title_rect.Height(),
                                    size.cx,
                                    drive_rect.Height(),
                                    SWP_NOACTIVATE | SWP_NOZORDER);

        toolbar_.DeferWindowPos(dwp, nullptr,
                                0,
                                title_rect.Height() + drive_rect.Height(),
                                size.cx,
                                toolbar_rect.Height(),
                                SWP_NOACTIVATE | SWP_NOZORDER);

        int list_y = title_rect.Height() + toolbar_rect.Height() + drive_rect.Height();
        int list_height = size.cy - list_y - status_rect.Height();

        list_.DeferWindowPos(dwp, nullptr,
                             0,
                             list_y,
                             size.cx,
                             list_height,
                             SWP_NOACTIVATE | SWP_NOZORDER);

        status_.DeferWindowPos(dwp, nullptr,
                               0,
                               list_y + list_height,
                               size.cx,
                               status_rect.Height(),
                               SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnDrawItem(UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL& handled)
{
    LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);

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
            ::GetWindowTextW(dis->hwndItem, label, _countof(label));

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

    return 0;
}

LRESULT UiFileManagerPanel::OnGetDispInfo(int control_id,
                                          LPNMHDR hdr,
                                          BOOL& handled)
{
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);

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
            return 0;
    }

    header->hinst = GetModuleHandleW(nullptr);
    return TRUE;
}

int UiFileManagerPanel::GetKnownDriveObjectIndex(const std::string& path)
{
    if (drive_list_)
    {
        const int count = drive_list_->item_size();

        for (int object_index = 0; object_index < count; ++object_index)
        {
            if (_stricmp(path.c_str(), drive_list_->item(object_index).path().c_str()) == 0)
                return object_index;
        }
    }

    return -1;
}

LRESULT UiFileManagerPanel::OnDriveChange(WORD notify_code,
                                          WORD control_id,
                                          HWND control,
                                          BOOL& handled)
{
    if (!drive_list_)
        return 0;

    int selected_item = drive_combo_.GetCurSel();
    if (selected_item == CB_ERR)
        return 0;

    int object_index = drive_combo_.GetItemData(selected_item);

    MoveToDrive(object_index);
    return 0;
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

LRESULT UiFileManagerPanel::OnListDoubleClock(int control_id,
                                              LPNMHDR hdr,
                                              BOOL& handled)
{
    int item_index = GetItemUnderMousePointer();
    if (item_index == -1)
        return 0;

    int object_index = list_.GetItemData(item_index);

    if (!directory_list_)
    {
        if (!drive_list_)
            return 0;

        MoveToDrive(object_index);
        return 0;
    }

    if (!IsValidDirectoryObjectIndex(object_index))
        return 0;

    const proto::DirectoryListItem& item =
        directory_list_->item(object_index);

    if (item.type() == proto::DirectoryListItem::DIRECTORY)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          item.name());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderUp(WORD notify_code,
                                       WORD control_id,
                                       HWND control,
                                       BOOL& handled)
{
    if (!directory_list_ || !directory_list_->has_parent())
    {
        MoveToDrive(kComputerObjectIndex);
    }
    else
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          "..");
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderAdd(WORD notify_code,
                                        WORD control_id,
                                        HWND control,
                                        BOOL& handled)
{
    if (!directory_list_)
        return 0;

    CIcon folder_icon(GetDirectoryIcon());
    int icon_index = list_imagelist_.AddIcon(folder_icon);

    CString folder_name;
    folder_name.LoadStringW(IDS_FT_NEW_FOLDER);

    list_.SetFocus();

    int item_index = list_.AddItem(list_.GetItemCount(),
                                   0,
                                   folder_name,
                                   icon_index);

    list_.SetItemData(item_index, kNewFolderObjectIndex);
    list_.EditLabel(item_index);

    return 0;
}

LRESULT UiFileManagerPanel::OnRefresh(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    delegate_->OnDriveListRequest(panel_type_);

    if (directory_list_)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          directory_list_->path(),
                                          std::string());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnRemove(WORD notify_code,
                                     WORD control_id,
                                     HWND control,
                                     BOOL& handled)
{
    if (!directory_list_)
        return 0;

    UINT selected_count = list_.GetSelectedCount();
    if (!selected_count)
        return 0;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;

    std::wstring object_list;

    if (selected_count == 1)
    {
        int selected_item = list_.GetNextItem(-1, LVNI_SELECTED);
        if (selected_item == -1)
            return 0;

        int object_index = list_.GetItemData(selected_item);

        if (!IsValidDirectoryObjectIndex(object_index))
            return 0;

        const proto::DirectoryListItem& item =
            directory_list_->item(object_index);

        object_list = UNICODEfromUTF8(item.name());

        if (item.type() == proto::DirectoryListItem::DIRECTORY)
            message.Format(IDS_FT_DELETE_CONFORM_DIR, object_list.c_str());
        else
            message.Format(IDS_FT_DELETE_CONFORM_FILE, object_list.c_str());
    }
    else
    {
        for (int item_index = list_.GetNextItem(-1, LVNI_SELECTED);
             item_index != -1;
             item_index = list_.GetNextItem(item_index, LVNI_SELECTED))
        {
            int object_index = list_.GetItemData(item_index);

            if (!IsValidDirectoryObjectIndex(object_index))
                continue;

            const proto::DirectoryListItem& item =
                directory_list_->item(object_index);

            object_list.append(UNICODEfromUTF8(item.name()));
            object_list.append(L"\r\n");
        }

        message.Format(IDS_FT_DELETE_CONFORM_MULTI, object_list.c_str());
    }

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        for (int item_index = list_.GetNextItem(-1, LVNI_SELECTED);
             item_index != -1;
             item_index = list_.GetNextItem(item_index, LVNI_SELECTED))
        {
            int object_index = list_.GetItemData(item_index);
            if (!IsValidDirectoryObjectIndex(object_index))
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

    return 0;
}

void UiFileManagerPanel::MoveToDrive(int object_index)
{
    int item_index = SelectDriveByObjectIndex(object_index);

    if (object_index == kComputerObjectIndex)
    {
        list_.DeleteAllItems();
        list_imagelist_.RemoveAll();
        directory_list_.reset();

        SetComputerViews();

        const int object_count = drive_list_->item_size();

        for (int object_index = 0; object_index < object_count; ++object_index)
        {
            const proto::DriveListItem& item = drive_list_->item(object_index);

            CIcon icon(GetDriveIcon(item.type()));
            int icon_index = list_imagelist_.AddIcon(icon);

            CString display_name(GetDriveDisplayName(item));

            int item_index = list_.AddItem(list_.GetItemCount(), 0, display_name, icon_index);
            list_.SetItemData(item_index, object_index);
            list_.SetItemText(item_index, 1, GetDriveDescription(item.type()));

            if (item.total_space())
                list_.SetItemText(item_index, 2, SizeToString(item.total_space()).c_str());

            if (item.free_space())
                list_.SetItemText(item_index, 3, SizeToString(item.free_space()).c_str());
        }
    }
    else if (object_index == kCurrentFolderObjectIndex)
    {
        WCHAR path[MAX_PATH];

        if (drive_combo_.GetItemText(item_index, path, _countof(path)))
        {
            delegate_->OnDirectoryListRequest(panel_type_,
                                              UTF8fromUNICODE(path),
                                              std::string());
        }
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

LRESULT UiFileManagerPanel::OnListEndLabelEdit(int control_id,
                                               LPNMHDR hdr,
                                               BOOL& handled)
{
    LPNMLVDISPINFOW disp_info = reinterpret_cast<LPNMLVDISPINFOW>(hdr);

    if (!directory_list_)
        return 0;

    int object_index = disp_info->item.lParam;

    // New folder.
    if (object_index == kNewFolderObjectIndex)
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
        DCHECK(IsValidDirectoryObjectIndex(object_index));

        // User canceled rename.
        if (!disp_info->item.pszText)
            return 0;

        delegate_->OnRenameRequest(panel_type_,
                                   directory_list_->path(),
                                   directory_list_->item(object_index).name(),
                                   UTF8fromUNICODE(disp_info->item.pszText));
    }

    delegate_->OnDirectoryListRequest(panel_type_,
                                      directory_list_->path(),
                                      std::string());
    return 0;
}

LRESULT UiFileManagerPanel::OnListItemChanged(int control_id,
                                              LPNMHDR hdr,
                                              BOOL& handled)
{
    UINT count = list_.GetSelectedCount();

    if (directory_list_)
    {
        bool enable = (count != 0);

        toolbar_.EnableButton(ID_DELETE, enable);
        toolbar_.EnableButton(ID_SEND, enable);
    }

    CString status;
    status.Format(IDS_FT_SELECTED_OBJECT_COUNT, count);
    status_.SetWindowTextW(status);

    return 0;
}

LRESULT UiFileManagerPanel::OnSend(WORD notify_code,
                                   WORD control_id,
                                   HWND control,
                                   BOOL& handled)
{
    if (!directory_list_)
        return 0;

    UINT selected_count = list_.GetSelectedCount();
    if (!selected_count)
        return 0;

    if (selected_count == 1)
    {
        int selected_item = list_.GetNextItem(-1, LVNI_SELECTED);
        if (selected_item == -1)
            return 0;

        int object_index = list_.GetItemData(selected_item);
        if (object_index < 0 || object_index >= directory_list_->item_size())
            return 0;

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

    return 0;
}

LRESULT UiFileManagerPanel::OnDriveEndEdit(int control_id,
                                           LPNMHDR hdr,
                                           BOOL& handled)
{
    PNMCBEENDEDITW end_edit = reinterpret_cast<PNMCBEENDEDITW>(hdr);

    if (end_edit->fChanged && end_edit->iWhy == CBENF_RETURN && end_edit->szText)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          UTF8fromUNICODE(end_edit->szText),
                                          std::string());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnHome(WORD notify_code,
                                   WORD control_id,
                                   HWND control,
                                   BOOL& handled)
{
    MoveToDrive(kComputerObjectIndex);
    return 0;
}

} // namespace aspia
