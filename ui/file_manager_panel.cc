//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager_panel.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "base/scoped_gdi_object.h"

namespace aspia {

bool FileManagerPanel::CreatePanel(HWND parent, Type type)
{
    type_ = type;

    const Module& module = Module::Current();

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

void FileManagerPanel::OnCreate()
{
    const Module& module = Module().Current();

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
                                           WC_COMBOBOXW,
                                           L"",
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                               WS_VSCROLL | CBS_DROPDOWN,
                                           0, 0, 200, 200,
                                           hwnd(),
                                           nullptr,
                                           module.Handle(),
                                           nullptr));
    address_window_.SetFont(default_font);

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

    list_window_.Attach(CreateWindowExW(WS_EX_CLIENTEDGE,
                                        WC_LISTVIEWW,
                                        L"",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                            LVS_REPORT | LVS_SHOWSELALWAYS,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        hwnd(),
                                        nullptr,
                                        module.Handle(),
                                        nullptr));

    list_window_.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);

    list_window_.AddColumn(module.string(IDS_FT_COLUMN_NAME), 200);
    list_window_.AddColumn(module.string(IDS_FT_COLUMN_SIZE), 80);
    list_window_.AddColumn(module.string(IDS_FT_COLUMN_TYPE), 100);

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
}

void FileManagerPanel::OnDestroy()
{
    list_window_.DestroyWindow();
    toolbar_window_.DestroyWindow();
    title_window_.DestroyWindow();
    address_window_.DestroyWindow();
    status_window_.DestroyWindow();
}

void FileManagerPanel::OnSize(int width, int height)
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

void FileManagerPanel::OnDrawItem(LPDRAWITEMSTRUCT dis)
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

void FileManagerPanel::OnGetDispInfo(LPNMHDR phdr)
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

    LoadStringW(Module().Current().Handle(),
                string_id,
                header->szText,
                _countof(header->szText));
}

bool FileManagerPanel::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result)
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

            switch (header->code)
            {
                case TTN_GETDISPINFO:
                    OnGetDispInfo(header);
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
