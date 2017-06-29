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

namespace aspia {

static const int kNewFolderIndex = -1;
static const int kComputerIndex = -1;
static const int kCurrentFolderIndex = -2;

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

    ScopedHICON icon(GetComputerIcon());
    DCHECK(icon.IsValid());

    int icon_index = drive_imagelist_.AddIcon(icon);

    int root_index =
        drive_combo_.AddItem(module_.String(IDS_FT_COMPUTER),
                             icon_index, 0, kComputerIndex);

    const int count = drive_list_->item_size();

    for (int index = 0; index < count; ++index)
    {
        const proto::DriveListItem& item = drive_list_->item(index);

        icon.Reset(GetDriveIcon(item.type()));
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

        int item_index = list_.AddItem(name, index, icon_index);
        list_.SetItemText(item_index, 2, GetDirectoryTypeString(name));
        list_.SetItemText(item_index, 3, TimeToString(item.modified()));
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

        int item_index = list_.AddItem(name, index, icon_index);
        list_.SetItemText(item_index, 1, SizeToString(item.size()));
        list_.SetItemText(item_index, 2, GetFileTypeString(name));
        list_.SetItemText(item_index, 3, TimeToString(item.modified()));
    }
}

void UiFileManagerPanel::SetComputerViews()
{
    list_.DeleteAllColumns();

    list_.AddColumn(module_.String(IDS_FT_COLUMN_NAME), 130);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_TYPE), 150);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_TOTAL_SPACE), 80);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_FREE_SPACE), 80);

    list_.ModifyStyle(0, LVS_SINGLESEL);

    toolbar_.EnableButton(ID_FOLDER_ADD, false);
    toolbar_.EnableButton(ID_FOLDER_UP, false);
    toolbar_.EnableButton(ID_DELETE, false);
    toolbar_.EnableButton(ID_SEND, false);
    toolbar_.EnableButton(ID_HOME, false);

    OnListItemChanged();
}

void UiFileManagerPanel::SetFolderViews()
{
    list_.DeleteAllColumns();

    list_.AddColumn(module_.String(IDS_FT_COLUMN_NAME), 180);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_SIZE), 70);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_TYPE), 100);
    list_.AddColumn(module_.String(IDS_FT_COLUMN_MODIFIED), 100);

    list_.ModifyStyle(LVS_SINGLESEL, 0);

    toolbar_.EnableButton(ID_FOLDER_ADD, true);
    toolbar_.EnableButton(ID_FOLDER_UP, true);
    toolbar_.EnableButton(ID_DELETE, true);
    toolbar_.EnableButton(ID_SEND, true);
    toolbar_.EnableButton(ID_HOME, true);

    OnListItemChanged();
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

    title_.Attach(CreateWindowExW(0,
                                  WC_STATICW,
                                  panel_name.c_str(),
                                  WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                  0, 0, 200, 20,
                                  hwnd(),
                                  nullptr,
                                  module_.Handle(),
                                  nullptr));
    title_.SetFont(default_font);

    drive_combo_.Create(hwnd(),
                        IDC_ADDRESS_COMBO, WS_VSCROLL | CBS_DROPDOWN,
                        module_.Handle());
    drive_combo_.SetFont(default_font);

    if (drive_imagelist_.CreateSmall())
        drive_combo_.SetImageList(drive_imagelist_);

    toolbar_.Create(hwnd(),
                    TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS,
                    module_.Handle());

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

    if (toolbar_imagelist_.CreateSmall())
    {
        toolbar_imagelist_.AddIcon(module_, IDI_REFRESH);
        toolbar_imagelist_.AddIcon(module_, IDI_DELETE);
        toolbar_imagelist_.AddIcon(module_, IDI_FOLDER_ADD);
        toolbar_imagelist_.AddIcon(module_, IDI_FOLDER_UP);
        toolbar_imagelist_.AddIcon(module_, IDI_HOME);

        if (panel_type_ == PanelType::LOCAL)
        {
            toolbar_imagelist_.AddIcon(module_, IDI_SEND);
        }
        else
        {
            DCHECK(panel_type_ == PanelType::REMOTE);
            toolbar_imagelist_.AddIcon(module_, IDI_RECIEVE);
        }

        toolbar_.SetImageList(toolbar_imagelist_);
    }

    if (panel_type_ == PanelType::LOCAL)
        toolbar_.SetButtonText(ID_SEND, module_.String(IDS_FT_SEND));
    else
        toolbar_.SetButtonText(ID_SEND, module_.String(IDS_FT_RECIEVE));

    list_.Create(hwnd(),
                 WS_EX_CLIENTEDGE,
                 LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EDITLABELS,
                 module_.Handle());

    list_.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);

    status_.Attach(CreateWindowExW(0,
                                   WC_STATICW,
                                   L"",
                                   WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                   0, 0, 200, 20,
                                   hwnd(),
                                   nullptr,
                                   module_.Handle(),
                                   nullptr));
    status_.SetFont(default_font);

    if (list_imagelist_.CreateSmall())
    {
        list_.SetImageList(list_imagelist_, LVSIL_SMALL);
    }

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

        int address_height = drive_combo_.Height();
        int toolbar_height = toolbar_.Height();
        int title_height = title_.Height();
        int status_height = status_.Height();

        DeferWindowPos(dwp, title_, nullptr,
                       0,
                       0,
                       width,
                       title_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, drive_combo_, nullptr,
                       0,
                       title_height,
                       width,
                       address_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp, toolbar_, nullptr,
                       0,
                       title_height + address_height,
                       width,
                       toolbar_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        int list_y = title_height + toolbar_height + address_height;
        int list_height = height - list_y - status_height;

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
                       status_height,
                       SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }
}

void UiFileManagerPanel::OnDrawItem(LPDRAWITEMSTRUCT dis)
{
    if (dis->hwndItem == title_.hwnd() || dis->hwndItem == status_.hwnd())
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

            int item_index = list_.AddItem(display_name, index, icon_index);
            list_.SetItemText(item_index, 1, GetDriveDescription(item.type()));

            if (item.total_space())
                list_.SetItemText(item_index, 2, SizeToString(item.total_space()));

            if (item.free_space())
                list_.SetItemText(item_index, 3, SizeToString(item.free_space()));
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

void UiFileManagerPanel::OnDirectoryChange()
{
    int item_index = list_.GetItemUnderPointer();
    if (item_index == -1)
        return;

    int object_index = list_.GetItemData<int>(item_index);

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
        list_.AddItem(folder_name, kNewFolderIndex, icon_index);

    list_.SetItemText(item_index, 2, GetDirectoryTypeString(folder_name));
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
        int selected_item = list_.GetFirstSelectedItem();
        if (selected_item == -1)
            return;

        int object_index = list_.GetItemData<int>(selected_item);
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

        for (int item_index = list_.GetFirstSelectedItem();
             item_index != -1;
             item_index = list_.GetNextSelectedItem(item_index))
        {
            int object_index = list_.GetItemData<int>(item_index);
            if (object_index < 0 || object_index >= directory_list_->item_size())
                continue;

            const proto::DirectoryListItem& item =
                directory_list_->item(object_index);

            object_list.append(UNICODEfromUTF8(item.name()));
            object_list.append(L"\r\n");
        }
    }

    std::wstring message = StringPrintfW(format.c_str(), object_list.c_str());

    if (MessageBoxW(hwnd(),
                    message.c_str(),
                    title.c_str(),
                    MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        for (int item_index = list_.GetFirstSelectedItem();
             item_index != -1;
             item_index = list_.GetNextSelectedItem(item_index))
        {
            int object_index = list_.GetItemData<int>(item_index);
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
        delegate_->OnCreateDirectoryRequest(panel_type_,
                                            directory_list_->path(),
                                            UTF8fromUNICODE(list_.GetTextFromEdit()));
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

    status_.SetWindowString(status);
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
        int selected_item = list_.GetFirstSelectedItem();
        if (selected_item == -1)
            return;

        int object_index = list_.GetItemData<int>(selected_item);
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
            else if (header->hwndFrom == list_.hwnd())
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
