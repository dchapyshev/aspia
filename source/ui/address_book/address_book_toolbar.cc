//
// PROJECT:         Aspia
// FILE:            ui/address_book/address_book_toolbar.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_toolbar.h"
#include "ui/resource.h"

namespace aspia {

bool AddressBookToolbar::Create(HWND parent)
{
    const DWORD kStyle = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;

    if (!CWindowImpl<AddressBookToolbar, CToolBarCtrl>::Create(
        parent, rcDefault, nullptr, kStyle))
    {
        return false;
    }

    SetExtendedStyle(TBSTYLE_EX_DOUBLEBUFFER);

    const BYTE kSessionButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECKGROUP;
    const BYTE kButtonStyle = BTNS_BUTTON | BTNS_AUTOSIZE;

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        {  0, ID_NEW,                       TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        {  1, ID_OPEN,                      TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        {  2, ID_SAVE,                      0,                                 kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        {  3, ID_ADD_GROUP,                 0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  4, ID_DELETE_GROUP,              0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  5, ID_EDIT_GROUP,                0,                                 kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        {  6, ID_ADD_COMPUTER,              0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  7, ID_DELETE_COMPUTER,           0,                                 kButtonStyle,        { 0 }, 0, -1 },
        {  8, ID_EDIT_COMPUTER,             0,                                 kButtonStyle,        { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        {  9, ID_DESKTOP_MANAGE_SESSION_TB, TBSTATE_ENABLED | TBSTATE_CHECKED, kSessionButtonStyle, { 0 }, 0, -1 },
        { 10, ID_DESKTOP_VIEW_SESSION_TB,   TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 11, ID_FILE_TRANSFER_SESSION_TB,  TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 12, ID_SYSTEM_INFO_SESSION_TB,    TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { 13, ID_POWER_MANAGE_SESSION_TB,   TBSTATE_ENABLED,                   kSessionButtonStyle, { 0 }, 0, -1 },
        { -1, 0,                            TBSTATE_ENABLED,                   BTNS_SEP,            { 0 }, 0, -1 },
        { 14, ID_ABOUT,                     TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
        { 15, ID_EXIT,                      TBSTATE_ENABLED,                   kButtonStyle,        { 0 }, 0, -1 },
    };

    SetButtonStructSize(sizeof(kButtons[0]));
    AddButtons(_countof(kButtons), kButtons);

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx, small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);

        auto add_icon = [&](UINT icon_id)
        {
            CIcon icon(AtlLoadIconImage(icon_id,
                                        LR_CREATEDIBSECTION,
                                        small_icon_size.cx,
                                        small_icon_size.cy));
            imagelist_.AddIcon(icon);
        };

        add_icon(IDI_DOCUMENT);
        add_icon(IDI_OPEN);
        add_icon(IDI_DISK);
        add_icon(IDI_FOLDER_ADD);
        add_icon(IDI_FOLDER_MINUS);
        add_icon(IDI_FOLDER_PENCIL);
        add_icon(IDI_COMPUTER_PLUS);
        add_icon(IDI_COMPUTER_MINUS);
        add_icon(IDI_COMPUTER_PENCIL);
        add_icon(IDI_MONITOR_WITH_KEYBOARD);
        add_icon(IDI_MONITOR);
        add_icon(IDI_FOLDER_STAND);
        add_icon(IDI_SYSTEM_MONITOR);
        add_icon(IDI_CONTROL_POWER);
        add_icon(IDI_ABOUT);
        add_icon(IDI_EXIT);
    }

    return true;
}

LRESULT AddressBookToolbar::OnGetDispInfo(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_NEW:
            string_id = IDS_TOOLTIP_NEW;
            break;

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

        case ID_ADD_GROUP:
            string_id = IDS_TOOLTIP_ADD_GROUP;
            break;

        case ID_DELETE_GROUP:
            string_id = IDS_TOOLTIP_DELETE_GROUP;
            break;

        case ID_EDIT_GROUP:
            string_id = IDS_TOOLTIP_EDIT_GROUP;
            break;

        case ID_ADD_COMPUTER:
            string_id = IDS_TOOLTIP_ADD_COMPUTER;
            break;

        case ID_DELETE_COMPUTER:
            string_id = IDS_TOOLTIP_DELETE_COMPUTER;
            break;

        case ID_EDIT_COMPUTER:
            string_id = IDS_TOOLTIP_EDIT_COMPUTER;
            break;

        case ID_DESKTOP_MANAGE_SESSION_TB:
            string_id = IDS_SESSION_TYPE_DESKTOP_MANAGE;
            break;

        case ID_DESKTOP_VIEW_SESSION_TB:
            string_id = IDS_SESSION_TYPE_DESKTOP_VIEW;
            break;

        case ID_FILE_TRANSFER_SESSION_TB:
            string_id = IDS_SESSION_TYPE_FILE_TRANSFER;
            break;

        case ID_SYSTEM_INFO_SESSION_TB:
            string_id = IDS_SESSION_TYPE_SYSTEM_INFO;
            break;

        case ID_POWER_MANAGE_SESSION_TB:
            string_id = IDS_SESSION_TYPE_POWER_MANAGE;
            break;

        default:
            return FALSE;
    }

    AtlLoadString(string_id, header->szText, _countof(header->szText));
    return TRUE;
}

} // namespace aspia
