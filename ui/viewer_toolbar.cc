//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/viewer_toolbar.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/viewer_toolbar.h"
#include "ui/resource.h"
#include "base/logging.h"

namespace aspia {

bool UiViewerToolBar::CreateViewerToolBar(HWND parent,
                                          proto::SessionType session_type)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT |
        TBSTYLE_LIST | TBSTYLE_TOOLTIPS;

    if (!Create(parent, 0, 0, style))
    {
        DLOG(ERROR) << "Unable to create viewer toolbar window: "
                    << GetLastSystemErrorString();
        return false;
    }

    if (session_type == proto::SessionType::SESSION_TYPE_DESKTOP_VIEW)
    {
        TBBUTTONINFOW button_info;

        button_info.cbSize = sizeof(button_info);
        button_info.dwMask = TBIF_STATE;
        button_info.fsState = 0;

        SetButtonInfo(ID_CAD, &button_info);
        SetButtonInfo(ID_SHORTCUTS, &button_info);
    }

    return true;
}

LRESULT UiViewerToolBar::OnCreate(UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam,
                                  BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS |
                     TBSTYLE_EX_MIXEDBUTTONS |
                     TBSTYLE_EX_DOUBLEBUFFER);

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        {  0, ID_CAD,        TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
        {  1, ID_SHORTCUTS,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_DROPDOWN, { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
        {  2, ID_AUTO_SIZE,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
        {  3, ID_FULLSCREEN, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECK,    { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
        {  4, ID_SETTINGS,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
        { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
        {  5, ID_ABOUT,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
        {  6, ID_EXIT,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 }
    };

    SetButtonStructSize(sizeof(kButtons[0]));
    AddButtons(_countof(kButtons), kButtons);

    CSize small_icon_size(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON));

    if (imagelist_.Create(small_icon_size.cx,
                          small_icon_size.cy,
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);

        AddIcon(IDI_CAD, small_icon_size);
        AddIcon(IDI_KEYS, small_icon_size);
        AddIcon(IDI_AUTOSIZE, small_icon_size);
        AddIcon(IDI_FULLSCREEN, small_icon_size);
        AddIcon(IDI_SETTINGS, small_icon_size);
        AddIcon(IDI_ABOUT, small_icon_size);
        AddIcon(IDI_EXIT, small_icon_size);
    }

    return ret;
}

LRESULT UiViewerToolBar::OnGetDispInfo(int control_id,
                                       LPNMHDR hdr,
                                       BOOL& handled)
{
    LPNMTTDISPINFOW header = reinterpret_cast<LPNMTTDISPINFOW>(hdr);

    switch (header->hdr.idFrom)
    {
        case ID_ABOUT:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_ABOUT);
            break;

        case ID_AUTO_SIZE:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_AUTO_SIZE);
            break;

        case ID_FULLSCREEN:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_FULLSCREEN);
            break;

        case ID_EXIT:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_EXIT);
            break;

        case ID_CAD:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_CAD);
            break;

        case ID_SHORTCUTS:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_SHORTCUTS);
            break;

        case ID_SETTINGS:
            header->lpszText = MAKEINTRESOURCEW(IDS_DM_TOOLTIP_SETTINGS);
            break;

        default:
            return 0;
    }

    header->hinst = GetModuleHandleW(nullptr);
    return 0;
}

void UiViewerToolBar::AddIcon(UINT icon_id, const CSize& icon_size)
{
    CIcon icon = AtlLoadIconImage(icon_id,
                                  LR_CREATEDIBSECTION,
                                  icon_size.cx,
                                  icon_size.cy);
    imagelist_.AddIcon(icon);
}

} // namespace aspia
