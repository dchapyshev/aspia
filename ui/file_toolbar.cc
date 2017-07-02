//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_toolbar.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_toolbar.h"
#include "ui/resource.h"
#include "base/logging.h"

namespace aspia {

UiFileToolBar::UiFileToolBar(Type type) :
    type_(type)
{
    // Nothing
}

bool UiFileToolBar::CreateFileToolBar(HWND parent)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT |
        TBSTYLE_LIST | TBSTYLE_TOOLTIPS;

    if (!Create(parent, CWindow::rcDefault, nullptr, style))
    {
        DLOG(ERROR) << "Unable to create file toolbar window: "
                    << GetLastSystemErrorString();
        return false;
    }

    return true;
}

LRESULT UiFileToolBar::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_DOUBLEBUFFER);

    TBBUTTON kButtons[] =
    {
        // iBitmap, idCommand, fsState, fsStyle, bReserved[2], dwData, iString
        { 0, ID_REFRESH,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 1, ID_DELETE,     TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 2, ID_FOLDER_ADD, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 3, ID_FOLDER_UP,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 4, ID_HOME,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE,{ 0 }, 0, -1 },
        { 5, ID_SEND,       TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT,{ 0 }, 0, -1 }
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

        AddIcon(IDI_REFRESH, small_icon_size);
        AddIcon(IDI_DELETE, small_icon_size);
        AddIcon(IDI_FOLDER_ADD, small_icon_size);
        AddIcon(IDI_FOLDER_UP, small_icon_size);
        AddIcon(IDI_HOME, small_icon_size);

        if (type_ == Type::LOCAL)
        {
            AddIcon(IDI_SEND, small_icon_size);
            SetButtonText(ID_SEND, IDS_FT_SEND);
        }
        else
        {
            DCHECK(type_ == Type::REMOTE);
            AddIcon(IDI_RECIEVE, small_icon_size);
            SetButtonText(ID_SEND, IDS_FT_RECIEVE);
        }
    }

    return ret;
}

LRESULT UiFileToolBar::OnGetDispInfo(int control_id, LPNMHDR hdr, BOOL& handled)
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
            if (type_ == Type::LOCAL)
            {
                header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_SEND);
            }
            else
            {
                DCHECK(type_ == Type::REMOTE);
                header->lpszText = MAKEINTRESOURCEW(IDS_FT_TOOLTIP_RECIEVE);
            }
        }
        break;

        default:
            return 0;
    }

    header->hinst = GetModuleHandleW(nullptr);
    return TRUE;
}

void UiFileToolBar::AddIcon(UINT icon_id, const CSize& icon_size)
{
    CIcon icon(AtlLoadIconImage(icon_id,
                                LR_CREATEDIBSECTION,
                                icon_size.cx,
                                icon_size.cy));
    imagelist_.AddIcon(icon);
}

void UiFileToolBar::SetButtonText(int command_id, UINT resource_id)
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
