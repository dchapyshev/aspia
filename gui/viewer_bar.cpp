//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/viewer_bar.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/viewer_bar.h"

#include "resource.h"

namespace aspia {

static TBBUTTON kButtons[] =
{
    // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
    {  0, ID_POWER,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_DROPDOWN, { 0 }, 0, -1 },
    { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
    {  1, ID_CAD,        TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    {  2, ID_SHORTCUTS,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_DROPDOWN, { 0 }, 0, -1 },
    { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
    {  3, ID_BELL,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    {  4, ID_CLIP_RECV,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    {  5, ID_CLIP_SEND,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
    {  6, ID_AUTO_SIZE,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    {  7, ID_FULLSCREEN, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_CHECK,    { 0 }, 0, -1 },
    { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
    {  8, ID_SETTINGS,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    { -1, 0,             TBSTATE_ENABLED, BTNS_SEP,                                    { 0 }, 0, -1 },
    {  9, ID_ABOUT,      TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
    { 10, ID_EXIT,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,                 { 0 }, 0, -1 },
};

LRESULT ViewerBar::OnCreate(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
{
    SetStyle(GetStyle() | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS);

    SetExtendedStyle(GetExtendedStyle() | TBSTYLE_EX_DRAWDDARROWS);

    SetButtonStructSize(sizeof(kButtons[0]));
    AddButtons(ARRAYSIZE(kButtons), kButtons);

    CSize icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    imagelist_.Create(icon_size.cx, icon_size.cy, ILC_MASK | ILC_COLOR32, 1, 1);

    AddIcon(icon_size, IDI_POWER);
    AddIcon(icon_size, IDI_CAD);
    AddIcon(icon_size, IDI_KEYS);
    AddIcon(icon_size, IDI_BELL);
    AddIcon(icon_size, IDI_CLIP_RECV);
    AddIcon(icon_size, IDI_CLIP_SEND);
    AddIcon(icon_size, IDI_IDEALSIZE);
    AddIcon(icon_size, IDI_FULLSCREEN);
    AddIcon(icon_size, IDI_SETTINGS);
    AddIcon(icon_size, IDI_ABOUT);
    AddIcon(icon_size, IDI_EXIT);

    SetImageList(imagelist_);

    return 0;
}

void ViewerBar::AddIcon(const CSize &icon_size, DWORD resource_id)
{
    imagelist_.AddIcon(CIcon(AtlLoadIconImage(resource_id,
                                              LR_CREATEDIBSECTION,
                                              icon_size.cx,
                                              icon_size.cy)));
}

LRESULT ViewerBar::OnGetDispInfo(int ctrl_id, LPNMHDR phdr, BOOL &handled)
{
    LPTOOLTIPTEXTW header = reinterpret_cast<LPTOOLTIPTEXTW>(phdr);
    UINT string_id = -1;

    switch (header->hdr.idFrom)
    {
        case ID_POWER:      string_id = IDS_POWER;      break;
        case ID_BELL:       string_id = IDS_BELL;       break;
        case ID_ABOUT:      string_id = IDS_ABOUT;      break;
        case ID_CLIP_RECV:  string_id = IDS_CLIP_RECV;  break;
        case ID_CLIP_SEND:  string_id = IDS_CLIP_SEND;  break;
        case ID_AUTO_SIZE:  string_id = IDS_AUTO_SIZE;  break;
        case ID_FULLSCREEN: string_id = IDS_FULLSCREEN; break;
        case ID_EXIT:       string_id = IDS_EXIT;       break;
        case ID_CAD:        string_id = IDS_CAD;        break;
        case ID_SHORTCUTS:  string_id = IDS_SHORTCUTS;  break;
        case ID_SETTINGS:   string_id = IDS_SETTINGS;   break;

        default:
            return 0;
    }

    AtlLoadString(string_id, header->szText, ARRAYSIZE(header->szText));

    return 0;
}

} // namespace aspia
