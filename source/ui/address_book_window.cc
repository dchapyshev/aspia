//
// PROJECT:         Aspia
// FILE:            ui/address_book_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book_window.h"
#include "ui/about_dialog.h"
#include "ui/computer_dialog.h"
#include "ui/computer_group_dialog.h"

namespace aspia {

bool AddressBookWindow::Dispatch(const NativeEvent& event)
{
    TranslateMessage(&event);
    DispatchMessageW(&event);
    return true;
}

LRESULT AddressBookWindow::OnCreate(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    SetWindowPos(nullptr, 0, 0, 980, 700, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   small_icon_size.cx,
                                   small_icon_size.cy);
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    CString title;
    title.LoadStringW(IDS_ADDRESS_BOOK_TITLE);
    SetWindowTextW(title);

    statusbar_.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP);
    int part_width = 240;
    statusbar_.SetParts(1, &part_width);

    CRect client_rect;
    GetClientRect(client_rect);

    splitter_.Create(*this, client_rect, nullptr,
                     WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

    splitter_.SetActivePane(SPLIT_PANE_LEFT);
    splitter_.SetSplitterPos(230);
    splitter_.m_cxySplitBar = 5;
    splitter_.m_cxyMin = 0;
    splitter_.m_bFullDrag = false;

    splitter_.SetSplitterExtendedStyle(splitter_.GetSplitterExtendedStyle() &~SPLIT_PROPORTIONAL);

    InitGroupTree();
    InitComputerList();

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, group_tree_ctrl_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, computer_list_ctrl_);

    InitToolBar(small_icon_size);

    return 0;
}

LRESULT AddressBookWindow::OnDestroy(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnSize(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    const CSize size(static_cast<DWORD>(lparam));

    toolbar_.AutoSize();
    statusbar_.SendMessageW(WM_SIZE);

    CRect toolbar_rect;
    toolbar_.GetWindowRect(toolbar_rect);

    CRect statusbar_rect;
    statusbar_.GetWindowRect(statusbar_rect);

    splitter_.MoveWindow(0, toolbar_rect.Height(),
                         size.cx, size.cy - toolbar_rect.Height() - statusbar_rect.Height(),
                         FALSE);
    return 0;
}

LRESULT AddressBookWindow::OnGetMinMaxInfo(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT AddressBookWindow::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    splitter_.DestroyWindow();
    group_tree_ctrl_.DestroyWindow();
    computer_list_ctrl_.DestroyWindow();
    statusbar_.DestroyWindow();
    toolbar_.DestroyWindow();

    DestroyWindow();
    PostQuitMessage(0);

    return 0;
}

LRESULT AddressBookWindow::OnGetDispInfo(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_OPEN:
            string_id = IDS_TOOLTIP_OPEN;
            break;

        case ID_SAVE:
            string_id = IDS_TOOLTIP_SAVE;
            break;

        case ID_ABOUT:
            string_id = IDS_TOOLTIP_ABOUT;
            break;

        case ID_EXIT:
            string_id = IDS_TOOLTIP_EXIT;
            break;

        case ID_DESKTOP_MANAGE_SESSION:
            string_id = IDS_SESSION_TYPE_DESKTOP_MANAGE;
            break;

        case ID_DESKTOP_VIEW_SESSION:
            string_id = IDS_SESSION_TYPE_DESKTOP_VIEW;
            break;

        case ID_FILE_TRANSFER_SESSION:
            string_id = IDS_SESSION_TYPE_FILE_TRANSFER;
            break;

        case ID_SYSTEM_INFO_SESSION:
            string_id = IDS_SESSION_TYPE_SYSTEM_INFO;
            break;

        case ID_POWER_SESSION:
            string_id = IDS_SESSION_TYPE_POWER_MANAGE;
            break;

        default:
            return FALSE;
    }

    AtlLoadString(string_id, header->szText, _countof(header->szText));
    return TRUE;
}

LRESULT AddressBookWindow::OnComputerListRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    CMenu menu;

    if (computer_list_ctrl_.GetSelectedCount() != 0)
        menu = AtlLoadMenu(IDR_COMPUTER_LIST_ITEM);
    else
        menu = AtlLoadMenu(IDR_COMPUTER_LIST);

    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    SetForegroundWindow(*this);

    CMenuHandle popup_menu(menu.GetSubMenu(0));
    popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);

    return 0;
}

LRESULT AddressBookWindow::OnGroupTreeRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    CMenu menu;

    if (group_tree_ctrl_.GetSelectedItem())
        menu = AtlLoadMenu(IDR_GROUP_TREE_ITEM);
    else
        menu = AtlLoadMenu(IDR_GROUP_TREE);

    POINT cursor_pos;
    GetCursorPos(&cursor_pos);

    SetForegroundWindow(*this);

    CMenuHandle popup_menu(menu.GetSubMenu(0));
    popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);

    return 0;
}

LRESULT AddressBookWindow::OnOpenButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnSaveButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnAboutButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    AboutDialog().DoModal();
    return 0;
}

LRESULT AddressBookWindow::OnExitButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    SendMessageW(WM_CLOSE);
    return 0;
}

LRESULT AddressBookWindow::OnAddComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    ComputerDialog().DoModal();
    return 0;
}

LRESULT AddressBookWindow::OnAddGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    ComputerGroupDialog().DoModal();
    return 0;
}

LRESULT AddressBookWindow::OnEditComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnEditGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnDeleteComputerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnDeleteGroupButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

LRESULT AddressBookWindow::OnSessionButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    return 0;
}

void AddressBookWindow::InitToolBar(const CSize& small_icon_size)
{
    const DWORD kStyle = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;

    toolbar_.Create(*this, rcDefault, nullptr, kStyle);
    toolbar_.SetExtendedStyle(TBSTYLE_EX_DOUBLEBUFFER);

    const BYTE kSessionButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECKGROUP;
    const BYTE kButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE;

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        {  0, ID_NEW,                    TBSTATE_ENABLED, kButtonStyle,        { 0 }, 0, -1 },
        {  1, ID_OPEN,                   TBSTATE_ENABLED, kButtonStyle,        { 0 }, 0, -1 },
        {  2, ID_SAVE,                   TBSTATE_ENABLED, kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                         TBSTATE_ENABLED, BTNS_SEP,            { 0 }, 0, -1 },
        {  3, ID_DESKTOP_MANAGE_SESSION, TBSTATE_ENABLED, kSessionButtonStyle, { 0 }, 0, -1 },
        {  4, ID_DESKTOP_VIEW_SESSION,   TBSTATE_ENABLED, kSessionButtonStyle, { 0 }, 0, -1 },
        {  5, ID_FILE_TRANSFER_SESSION,  TBSTATE_ENABLED, kSessionButtonStyle, { 0 }, 0, -1 },
        {  6, ID_SYSTEM_INFO_SESSION,    TBSTATE_ENABLED, kSessionButtonStyle, { 0 }, 0, -1 },
        {  7, ID_POWER_SESSION,          TBSTATE_ENABLED, kSessionButtonStyle, { 0 }, 0, -1 },
        { -1, 0,                         TBSTATE_ENABLED, BTNS_SEP,            { 0 }, 0, -1 },
        {  8, ID_ABOUT,                  TBSTATE_ENABLED, kButtonStyle,        { 0 }, 0, -1 },
        {  9, ID_EXIT,                   TBSTATE_ENABLED, kButtonStyle,        { 0 }, 0, -1 },
    };

    toolbar_.SetButtonStructSize(sizeof(kButtons[0]));
    toolbar_.AddButtons(_countof(kButtons), kButtons);

    if (toolbar_imagelist_.Create(small_icon_size.cx,
                                  small_icon_size.cy,
                                  ILC_MASK | ILC_COLOR32,
                                  1, 1))
    {
        toolbar_.SetImageList(toolbar_imagelist_);

        auto add_icon = [&](UINT icon_id)
        {
            CIcon icon(AtlLoadIconImage(icon_id,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy));
            toolbar_imagelist_.AddIcon(icon);
        };

        add_icon(IDI_DOCUMENT);
        add_icon(IDI_OPEN);
        add_icon(IDI_DISK);
        add_icon(IDI_MONITOR_WITH_KEYBOARD);
        add_icon(IDI_MONITOR);
        add_icon(IDI_FOLDER_STAND);
        add_icon(IDI_SYSTEM_MONITOR);
        add_icon(IDI_CONTROL_POWER);
        add_icon(IDI_ABOUT);
        add_icon(IDI_EXIT);
    }

    toolbar_.CheckButton(ID_DESKTOP_MANAGE_SESSION, TRUE);
}

void AddressBookWindow::InitComputerList()
{
    const DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_TABSTOP | LVS_SHOWSELALWAYS;

    computer_list_ctrl_.Create(splitter_, rcDefault, nullptr, style,
                               WS_EX_CLIENTEDGE, kComputerListCtrl);

    auto add_column = [&](UINT resource_id, int column_index, int width)
    {
        CString text;
        text.LoadStringW(resource_id);

        computer_list_ctrl_.AddColumn(text, column_index);
        computer_list_ctrl_.SetColumnWidth(column_index, width);
    };

    add_column(IDS_COL_NAME, 0, 250);
    add_column(IDS_COL_ADDRESS, 1, 200);
    add_column(IDS_COL_PORT, 2, 100);
}

void AddressBookWindow::InitGroupTree()
{
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES |
        TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_LINESATROOT;

    group_tree_ctrl_.Create(splitter_, rcDefault, nullptr, style,
                            WS_EX_CLIENTEDGE, kGroupTreeCtrl);
}

} // namespace aspia
