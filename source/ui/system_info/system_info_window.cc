//
// PROJECT:         Aspia
// FILE:            ui/system_info/system_info_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_path.h"
#include "base/strings/unicode.h"
#include "base/scoped_clipboard.h"
#include "base/scoped_select_object.h"
#include "base/scoped_hdc.h"
#include "base/logging.h"
#include "report/report_html_file.h"
#include "report/report_json_file.h"
#include "report/report_xml_file.h"
#include "ui/system_info/system_info_window.h"
#include "ui/system_info/category_select_dialog.h"
#include "ui/system_info/report_progress_dialog.h"
#include "ui/about_dialog.h"

#include <atldlgs.h>
#include <strsafe.h>

namespace aspia {

static const int kDefaultWindowWidth = 980;
static const int kDefaultWindowHeight = 700;

SystemInfoWindow::SystemInfoWindow(Delegate* delegate)
    : delegate_(delegate),
      category_list_(CreateCategoryTree())
{
    DCHECK(delegate_);
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

SystemInfoWindow::~SystemInfoWindow()
{
    GUITHREADINFO thread_info;
    thread_info.cbSize = sizeof(thread_info);

    if (GetGUIThreadInfo(ui_thread_.thread_id(), &thread_info))
    {
        ::PostMessageW(thread_info.hwndActive, WM_CLOSE, 0, 0);
    }

    ui_thread_.Stop();
}

void SystemInfoWindow::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    accelerator_.LoadAcceleratorsW(IDC_SYSTEM_INFO_ACCELERATORS);

    CString title;
    title.LoadStringW(IDS_SI_SYSTEM_INFORMATION);

    if (!Create(nullptr, rcDefault, title, WS_OVERLAPPEDWINDOW))
    {
        LOG(ERROR) << "System information window not created: " << GetLastSystemErrorString();
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOW);
        UpdateWindow();
    }
}

void SystemInfoWindow::OnThreadRunning(MessageLoop* message_loop)
{
    // Run message loop with this message dispatcher.
    message_loop->Run(this);
}

void SystemInfoWindow::OnAfterThreadRunning()
{
    DestroyWindow();
}

bool SystemInfoWindow::Dispatch(const NativeEvent& event)
{
    if (!accelerator_.TranslateAcceleratorW(*this, const_cast<NativeEvent*>(&event)))
    {
        TranslateMessage(&event);
        DispatchMessageW(&event);
    }

    return true;
}

LRESULT SystemInfoWindow::OnCreate(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
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

    const DWORD toolbar_style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT |
        TBSTYLE_LIST | TBSTYLE_TOOLTIPS;

    toolbar_.Create(*this, rcDefault, nullptr, toolbar_style);

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

    const DWORD tree_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES |
        TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_LINESATROOT;

    tree_.Create(splitter_, rcDefault, nullptr, tree_style, WS_EX_CLIENTEDGE, kTreeControl);

    const DWORD list_style = WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_TABSTOP | LVS_SHOWSELALWAYS;

    list_.Create(splitter_, rcDefault, nullptr, list_style, WS_EX_CLIENTEDGE, kListControl);

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, tree_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, list_);

    statusbar_.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP);
    int part_width = 240;
    statusbar_.SetParts(1, &part_width);

    SetWindowPos(nullptr, 0, 0, kDefaultWindowWidth, kDefaultWindowHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    tree_.AddChildItems(small_icon_size, category_list_, TVI_ROOT);
    tree_.ExpandChildGroups(TVI_ROOT);
    tree_.SelectItem(tree_.GetChildItem(TVI_ROOT));
    tree_.SetFocus();
    return 0;
}

LRESULT SystemInfoWindow::OnDestroy(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    toolbar_.DestroyWindow();
    tree_.DestroyWindow();
    list_.DestroyWindow();
    splitter_.DestroyWindow();
    statusbar_.DestroyWindow();
    return 0;
}

LRESULT SystemInfoWindow::OnSize(
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

LRESULT SystemInfoWindow::OnGetMinMaxInfo(
    UINT /* message */, WPARAM /* wparam */, LPARAM lparam, BOOL& /* handled */)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT SystemInfoWindow::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    delegate_->OnWindowClose();
    return 0;
}

void SystemInfoWindow::ShowDropDownMenu(int button_id, RECT* button_rect)
{
    if (button_id != ID_SAVE)
        return;

    if (toolbar_.MapWindowPoints(HWND_DESKTOP, button_rect))
    {
        TPMPARAMS tpm;
        tpm.cbSize = sizeof(TPMPARAMS);
        tpm.rcExclude = *button_rect;

        CMenu menu;
        menu.LoadMenuW(IDR_SAVE_REPORT);

        CMenuHandle pupup = menu.GetSubMenu(0);

        pupup.TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
                               button_rect->left,
                               button_rect->bottom,
                               *this,
                               &tpm);
    }
}

LRESULT SystemInfoWindow::OnToolBarDropDown(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTOOLBARW header = reinterpret_cast<LPNMTOOLBARW>(hdr);
    ShowDropDownMenu(header->iItem, &header->rcButton);
    return 0;
}

LRESULT SystemInfoWindow::OnCategorySelected(
    int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMTREEVIEWW nmtv = reinterpret_cast<LPNMTREEVIEWW>(hdr);
    Refresh(tree_.GetItemCategory(nmtv->itemNew.hItem));
    return 0;
}

LRESULT SystemInfoWindow::OnListRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    if (!list_.GetSelectedCount())
        return 0;

    CMenu menu(AtlLoadMenu(IDR_LIST_COPY));

    if (menu)
    {
        POINT cursor_pos;

        if (GetCursorPos(&cursor_pos))
        {
            SetForegroundWindow(*this);

            CMenuHandle popup_menu(menu.GetSubMenu(0));
            if (popup_menu)
            {
                if (list_.GetColumnCount() > 2)
                {
                    popup_menu.EnableMenuItem(ID_COPY_VALUE, MF_BYCOMMAND | MF_DISABLED);
                }

                popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);
            }
        }
    }

    return 0;
}

LRESULT SystemInfoWindow::OnListColumnClick(int /* control_id */, LPNMHDR hdr, BOOL& /* handled */)
{
    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(hdr);
    list_.SortColumnItems(pnmv->iSubItem);
    return 0;
}

LRESULT SystemInfoWindow::OnSaveSelectedButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (CategorySelectDialog(&category_list_).DoModal() == IDOK)
        Save(&category_list_);

    std::function<void(CategoryList&, bool)> set_check_state =
        [&](CategoryList& category_list, bool checked)
    {
        for (auto& category : category_list)
        {
            if (category->type() == Category::Type::INFO_LIST ||
                category->type() == Category::Type::INFO_PARAM_VALUE)
            {
                category->category_info()->SetChecked(checked);
            }
            else
            {
                set_check_state(*category->category_group()->mutable_child_list(), checked);
            }
        }
    };

    set_check_state(category_list_, false);
    return 0;
}

LRESULT SystemInfoWindow::OnSaveAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    std::function<void(CategoryList&, bool)> set_check_state =
        [&](CategoryList& category_list, bool checked)
    {
        for (auto& category : category_list)
        {
            if (category->type() == Category::Type::INFO_LIST ||
                category->type() == Category::Type::INFO_PARAM_VALUE)
            {
                category->category_info()->SetChecked(checked);
            }
            else
            {
                set_check_state(*category->category_group()->mutable_child_list(), checked);
            }
        }
    };

    set_check_state(category_list_, true);
    Save(&category_list_);
    set_check_state(category_list_, false);

    return 0;
}

LRESULT SystemInfoWindow::OnSaveCurrentButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    Category* category = tree_.GetItemCategory(tree_.GetSelectedItem());
    if (!category)
        return 0;

    if (category->type() == Category::Type::INFO_LIST ||
        category->type() == Category::Type::INFO_PARAM_VALUE)
    {
        category->category_info()->SetChecked(true);
        Save(&category_list_);
        category->category_info()->SetChecked(false);
    }

    return 0;
}

LRESULT SystemInfoWindow::OnScreenshotButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    ScopedGetDC window_dc(*this);
    if (!window_dc)
        return 0;

    CDC memory_dc;
    memory_dc.CreateCompatibleDC(window_dc);
    if (memory_dc.IsNull())
        return 0;

    CRect window_rect;
    if (!GetClientRect(window_rect))
        return 0;

    CBitmap bitmap = CreateCompatibleBitmap(window_dc,
                                            window_rect.Width(),
                                            window_rect.Height());
    if (bitmap.IsNull())
        return 0;

    ScopedSelectObject select_object(memory_dc, bitmap);

    if (memory_dc.BitBlt(0, 0, window_rect.Width(), window_rect.Height(),
                         window_dc, 0, 0, SRCCOPY))
    {
        if (OpenClipboard())
        {
            EmptyClipboard();
            SetClipboardData(CF_BITMAP, bitmap);
            CloseClipboard();
        }
    }

    return 0;
}

LRESULT SystemInfoWindow::OnRefreshButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    Refresh(tree_.GetItemCategory(tree_.GetSelectedItem()));
    return 0;
}

LRESULT SystemInfoWindow::OnCopyButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    LVITEMW item;
    memset(&item, 0, sizeof(item));

    if (!list_.GetSelectedItem(&item))
        return 0;

    std::wstring text = GetListHeaderText();
    text.append(L"\r\n");

    const int column_count = list_.GetColumnCount();

    for (int column_index = 0; column_index < column_count; ++column_index)
    {
        WCHAR buffer[256] = { 0 };

        if (list_.GetItemText(item.iItem, column_index, buffer, ARRAYSIZE(buffer)))
        {
            text.append(buffer);

            if (column_index < column_count - 1)
                text.append(L"\t");
        }
    }

    CopyTextToClipboard(text);

    return 0;
}

LRESULT SystemInfoWindow::OnCopyAllButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    std::wstring text = GetListHeaderText();

    const int item_count = list_.GetItemCount();
    const int column_count = list_.GetColumnCount();

    if (item_count > 0)
        text.append(L"\r\n");

    for (int item_index = 0; item_index < item_count; ++item_index)
    {
        for (int column_index = 0; column_index < column_count; ++column_index)
        {
            WCHAR buffer[256] = { 0 };

            if (list_.GetItemText(item_index, column_index, buffer, ARRAYSIZE(buffer)))
            {
                text.append(buffer);

                if (column_index < column_count - 1)
                    text.append(L"\t");
            }
        }

        if (item_index < item_count - 1)
            text.append(L"\r\n");
    }

    CopyTextToClipboard(text);

    return 0;
}

LRESULT SystemInfoWindow::OnCopyValueButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    LVITEMW item;
    memset(&item, 0, sizeof(item));

    if (!list_.GetSelectedItem(&item))
        return 0;

    WCHAR buffer[256] = { 0 };

    if (list_.GetItemText(item.iItem, 1, buffer, ARRAYSIZE(buffer)))
    {
        CopyTextToClipboard(buffer);
    }

    return 0;
}

LRESULT SystemInfoWindow::OnAboutButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    AboutDialog().DoModal(*this);
    return 0;
}

LRESULT SystemInfoWindow::OnExitButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void SystemInfoWindow::Save(CategoryList* category_list)
{
    static const WCHAR html_files[] = L"*.html;*.htm";
    static const WCHAR json_files[] = L"*.json";
    static const WCHAR xml_files[] = L"*.xml";

    WCHAR filter[256] = { 0 };
    int length = 0;

    length += AtlLoadString(IDS_SI_FILTER_HTML, filter, ARRAYSIZE(filter)) + 1;
    StringCbCatW(filter + length, ARRAYSIZE(filter) - length, html_files);
    length += ARRAYSIZE(html_files);

    length += AtlLoadString(IDS_SI_FILTER_JSON, filter + length, ARRAYSIZE(filter) - length) + 1;
    StringCbCatW(filter + length, ARRAYSIZE(filter) - length, json_files);
    length += ARRAYSIZE(json_files);

    length += AtlLoadString(IDS_SI_FILTER_XML, filter + length, ARRAYSIZE(filter) - length) + 1;
    StringCbCatW(filter + length, ARRAYSIZE(filter) - length, xml_files);

    WCHAR computer_name[256] = { 0 };
    DWORD computer_name_length = ARRAYSIZE(computer_name);
    GetComputerNameW(computer_name, &computer_name_length);

    CFileDialog save_dialog(FALSE, L"html", computer_name,
                            OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter);
    if (save_dialog.DoModal() == IDCANCEL)
        return;

    FilePath file_path = save_dialog.m_szFileName;
    FilePath extension = file_path.extension();

    std::unique_ptr<Report> report;

    if (extension == L".html" || extension == L".htm")
    {
        report = ReportHtmlFile::Create(file_path);
    }
    else if (extension == L".json")
    {
        report = ReportJsonFile::Create(file_path);
    }
    else if (extension == L".xml")
    {
        report = ReportXmlFile::Create(file_path);
    }
    else
    {
        LOG(WARNING) << "Invalid file extension: " << extension;
        return;
    }

    if (!report)
        return;

    ReportProgressDialog progress_dialog(category_list, report.get(),
                                         std::bind(&SystemInfoWindow::OnRequest, this,
                                                   std::placeholders::_1, std::placeholders::_2));
    progress_dialog.DoModal();
}

void SystemInfoWindow::Refresh(Category* category)
{
    if (!category)
        return;

    list_.DeleteAllItems();
    list_.DeleteAllColumns();

    statusbar_icon_ = AtlLoadIconImage(category->Icon(),
                                       LR_CREATEDIBSECTION,
                                       GetSystemMetrics(SM_CXSMICON),
                                       GetSystemMetrics(SM_CYSMICON));

    statusbar_.SetText(0, UNICODEfromUTF8(category->Name()).c_str());
    statusbar_.SetIcon(0, statusbar_icon_);

    if (category->type() == Category::Type::INFO_LIST ||
        category->type() == Category::Type::INFO_PARAM_VALUE)
    {
        CategoryInfo* category_info = category->category_info();

        category_info->SetChecked(true);

        ReportProgressDialog dialog(&category_list_, &list_,
                                    std::bind(&SystemInfoWindow::OnRequest, this,
                                              std::placeholders::_1, std::placeholders::_2));
        dialog.DoModal();
    }
    else
    {
        DCHECK(category->type() == Category::Type::GROUP);
        list_.EnableWindow(FALSE);
    }
}

std::wstring SystemInfoWindow::GetListHeaderText()
{
    std::wstring text;

    CHeaderCtrl header = list_.GetHeader();
    const int column_count = header.GetItemCount();

    for (int column_index = 0; column_index < column_count; ++column_index)
    {
        WCHAR buffer[256] = { 0 };
        HDITEMW header_item;

        memset(&header_item, 0, sizeof(header_item));
        header_item.mask       = HDI_TEXT;
        header_item.pszText    = buffer;
        header_item.cchTextMax = ARRAYSIZE(buffer);

        if (header.GetItem(column_index, &header_item))
        {
            text.append(header_item.pszText);

            if (column_index < column_count - 1)
                text.append(L"\t");
        }
    }

    return text;
}

void SystemInfoWindow::CopyTextToClipboard(const std::wstring& text)
{
    ScopedClipboard clipboard;
    if (!clipboard.Init(*this))
        return;

    clipboard.Empty();

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(WCHAR));
    if (!text_global)
        return;

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));
    if (!text_global_locked)
    {
        GlobalFree(text_global);
        return;
    }

    memcpy(text_global_locked, text.data(), text.length() * sizeof(WCHAR));
    text_global_locked[text.length()] = 0;

    GlobalUnlock(text_global);

    clipboard.SetData(CF_UNICODETEXT, text_global);
}

void SystemInfoWindow::OnRequest(
    std::string_view guid, std::shared_ptr<ReportCreatorProxy> report_creator)
{
    delegate_->OnRequest(guid, report_creator);
}

} // namespace aspia
