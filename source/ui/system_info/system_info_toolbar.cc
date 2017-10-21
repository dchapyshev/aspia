//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info_toolbar.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/system_info_toolbar.h"
#include "ui/resource.h"

namespace aspia {

LRESULT SystemInfoToolbar::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER);

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        { 0, ID_SAVE,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_DROPDOWN | BTNS_SHOWTEXT,{ 0 }, 0, -1 },
        { 1, ID_HOME,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 2, ID_REFRESH, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 3, ID_ABOUT,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 4, ID_EXIT,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
    };

    SetButtonStructSize(sizeof(kButtons[0]));
    AddButtons(_countof(kButtons), kButtons);

    CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx,
                          small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);

        AddIcon(IDI_DISK, small_icon_size);
        AddIcon(IDI_HOME, small_icon_size);
        AddIcon(IDI_REFRESH, small_icon_size);
        AddIcon(IDI_ABOUT, small_icon_size);
        AddIcon(IDI_EXIT, small_icon_size);

        SetButtonText(ID_SAVE, IDS_SI_SAVE_REPORT);
    }

    return 0;
}

LRESULT SystemInfoToolbar::OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(handled);

    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);
    UINT string_id;

    switch (header->hdr.idFrom)
    {
        case ID_SAVE:
            string_id = IDS_SI_TOOLTIP_SAVE;
            break;

        case ID_HOME:
            string_id = IDS_SI_TOOLTIP_HOME;
            break;

        case ID_REFRESH:
            string_id = IDS_SI_TOOLTIP_REFRESH;
            break;

        case ID_ABOUT:
            string_id = IDS_SI_TOOLTIP_ABOUT;
            break;

        case ID_EXIT:
            string_id = IDS_SI_TOOLTIP_EXIT;
            break;

        default:
            return 0;
    }

    AtlLoadString(string_id, header->szText, _countof(header->szText));
    return TRUE;
}

void SystemInfoToolbar::AddIcon(UINT icon_id, const CSize& icon_size)
{
    CIcon icon(AtlLoadIconImage(icon_id,
                                LR_CREATEDIBSECTION,
                                icon_size.cx,
                                icon_size.cy));
    imagelist_.AddIcon(icon);
}

void SystemInfoToolbar::SetButtonText(int command_id, UINT resource_id)
{
    int button_index = CommandToIndex(command_id);
    if (button_index == -1)
        return;

    TBBUTTON button = { 0 };
    if (!GetButton(button_index, &button))
        return;

    CString string;
    string.LoadStringW(resource_id);

    int string_id = AddStrings(string);
    if (string_id == -1)
        return;

    button.iString = string_id;

    DeleteButton(button_index);
    InsertButton(button_index, &button);
}

} // namespace aspia
