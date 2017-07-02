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

static_assert(UiFileList::kInvalidObjectIndex == UiDriveList::kInvalidObjectIndex,
              "Values must be equal");

UiFileManagerPanel::UiFileManagerPanel(PanelType panel_type,
                                       Delegate* delegate) :
    toolbar_(panel_type == PanelType::LOCAL ?
             UiFileToolBar::Type::LOCAL : UiFileToolBar::Type::REMOTE),
    panel_type_(panel_type),
    delegate_(delegate)
{
    // Nothing
}

void UiFileManagerPanel::ReadDriveList(std::unique_ptr<proto::DriveList> drive_list)
{
    drive_list_.Read(std::move(drive_list));

    if (!file_list_.HasDirectoryList())
    {
        MoveToDrive(UiDriveList::kComputerObjectIndex);
    }
}

void UiFileManagerPanel::ReadDirectoryList(
    std::unique_ptr<proto::DirectoryList> directory_list)
{
    toolbar_.EnableButton(ID_FOLDER_ADD, TRUE);
    toolbar_.EnableButton(ID_FOLDER_UP, TRUE);
    toolbar_.EnableButton(ID_HOME, TRUE);

    file_list_.Read(std::move(directory_list));
    drive_list_.SetCurrentPath(file_list_.CurrentPath());
}

LPARAM UiFileManagerPanel::OnCreate(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    HFONT default_font = AtlGetStockFont(DEFAULT_GUI_FONT);

    CString panel_name;

    if (panel_type_ == PanelType::LOCAL)
        panel_name.LoadStringW(IDS_FT_LOCAL_COMPUTER);
    else
        panel_name.LoadStringW(IDS_FT_REMOTE_COMPUTER);

    CRect title_rect(0, 0, 200, 20);
    title_.Create(*this, title_rect, panel_name, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    title_.SetFont(default_font);

    drive_list_.CreateDriveList(*this, kDriveListControl);
    toolbar_.CreateFileToolBar(*this);
    file_list_.CreateFileList(*this, kFileListControl);

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
    drive_list_.DestroyWindow();
    file_list_.DestroyWindow();
    toolbar_.DestroyWindow();
    title_.DestroyWindow();
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
        drive_list_.GetWindowRect(drive_rect);

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

        drive_list_.DeferWindowPos(dwp, nullptr,
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

        file_list_.DeferWindowPos(dwp, nullptr,
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

LRESULT UiFileManagerPanel::OnDriveChange(WORD notify_code,
                                          WORD control_id,
                                          HWND control,
                                          BOOL& handled)
{
    int object_index = drive_list_.SelectedObject();

    if (object_index == UiDriveList::kInvalidObjectIndex)
        return 0;

    MoveToDrive(object_index);
    return 0;
}

LRESULT UiFileManagerPanel::OnListDoubleClock(int control_id,
                                              LPNMHDR hdr,
                                              BOOL& handled)
{
    int object_index = file_list_.GetObjectUnderMousePointer();

    if (!file_list_.HasDirectoryList())
    {
        MoveToDrive(object_index);
        return 0;
    }

    if (!file_list_.IsValidObjectIndex(object_index))
        return 0;

    const proto::DirectoryListItem& item = file_list_.Object(object_index);

    if (item.type() == proto::DirectoryListItem::DIRECTORY)
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          file_list_.CurrentPath(),
                                          item.name());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderUp(WORD notify_code,
                                       WORD control_id,
                                       HWND control,
                                       BOOL& handled)
{
    if (!file_list_.HasParentDirectory())
    {
        MoveToDrive(UiDriveList::kComputerObjectIndex);
    }
    else
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          file_list_.CurrentPath(),
                                          "..");
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderAdd(WORD notify_code,
                                        WORD control_id,
                                        HWND control,
                                        BOOL& handled)
{
    file_list_.AddDirectory();
    return 0;
}

LRESULT UiFileManagerPanel::OnRefresh(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    delegate_->OnDriveListRequest(panel_type_);

    if (file_list_.HasDirectoryList())
    {
        delegate_->OnDirectoryListRequest(panel_type_,
                                          file_list_.CurrentPath(),
                                          std::string());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnRemove(WORD notify_code,
                                     WORD control_id,
                                     HWND control,
                                     BOOL& handled)
{
    if (!file_list_.HasDirectoryList())
        return 0;

    UINT selected_count = file_list_.GetSelectedCount();
    if (!selected_count)
        return 0;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;

    std::wstring object_list;

    if (selected_count == 1)
    {
        proto::DirectoryListItem* object = file_list_.FirstSelectedObject();
        if (!object)
            return 0;

        object_list = UNICODEfromUTF8(object->name());

        if (object->type() == proto::DirectoryListItem::DIRECTORY)
            message.Format(IDS_FT_DELETE_CONFORM_DIR, object_list.c_str());
        else
            message.Format(IDS_FT_DELETE_CONFORM_FILE, object_list.c_str());
    }
    else
    {
        for (UiFileList::Iterator iter(file_list_, UiFileList::Iterator::SELECTED);
             !iter.IsAtEnd();
             iter.Advance())
        {
            proto::DirectoryListItem* object = iter.Object();
            if (!object)
                continue;

            object_list.append(UNICODEfromUTF8(object->name()));
            object_list.append(L"\r\n");
        }

        message.Format(IDS_FT_DELETE_CONFORM_MULTI, object_list.c_str());
    }

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        for (UiFileList::Iterator iter(file_list_, UiFileList::Iterator::SELECTED);
             !iter.IsAtEnd();
             iter.Advance())
        {
            proto::DirectoryListItem* object = iter.Object();
            if (!object)
                continue;

            delegate_->OnRemoveRequest(panel_type_,
                                       file_list_.CurrentPath(),
                                       object->name());
        }

        delegate_->OnDirectoryListRequest(panel_type_,
                                          file_list_.CurrentPath(),
                                          std::string());
    }

    return 0;
}

void UiFileManagerPanel::MoveToDrive(int object_index)
{
    drive_list_.SelectObject(object_index);

    if (object_index == UiDriveList::kComputerObjectIndex)
    {
        toolbar_.EnableButton(ID_FOLDER_ADD, FALSE);
        toolbar_.EnableButton(ID_FOLDER_UP, FALSE);
        toolbar_.EnableButton(ID_DELETE, FALSE);
        toolbar_.EnableButton(ID_SEND, FALSE);
        toolbar_.EnableButton(ID_HOME, FALSE);

        file_list_.Read(drive_list_.DriveList());
        drive_list_.SetCurrentPath(file_list_.CurrentPath());
    }
    else
    {
        std::string path = drive_list_.ObjectPath(object_index);

        if (!path.empty())
        {
            delegate_->OnDirectoryListRequest(panel_type_,
                                              path,
                                              std::string());
        }
    }
}

LRESULT UiFileManagerPanel::OnListEndLabelEdit(int control_id,
                                               LPNMHDR hdr,
                                               BOOL& handled)
{
    LPNMLVDISPINFOW disp_info = reinterpret_cast<LPNMLVDISPINFOW>(hdr);

    if (!file_list_.HasDirectoryList())
        return 0;

    int object_index = disp_info->item.lParam;

    // New folder.
    if (object_index == UiFileList::kNewFolderObjectIndex)
    {
        CEdit edit(file_list_.GetEditControl());

        WCHAR buffer[MAX_PATH] = { 0 };
        edit.GetWindowTextW(buffer, _countof(buffer));

        delegate_->OnCreateDirectoryRequest(panel_type_,
                                            file_list_.CurrentPath(),
                                            UTF8fromUNICODE(buffer));
    }
    else // Rename exists item.
    {
        DCHECK(file_list_.IsValidObjectIndex(object_index));

        // User canceled rename.
        if (!disp_info->item.pszText)
            return 0;

        delegate_->OnRenameRequest(panel_type_,
                                   file_list_.CurrentPath(),
                                   file_list_.ObjectName(object_index),
                                   UTF8fromUNICODE(disp_info->item.pszText));
    }

    delegate_->OnDirectoryListRequest(panel_type_,
                                      file_list_.CurrentPath(),
                                      std::string());
    return 0;
}

LRESULT UiFileManagerPanel::OnListItemChanged(int control_id,
                                              LPNMHDR hdr,
                                              BOOL& handled)
{
    UINT count = file_list_.GetSelectedCount();

    if (file_list_.HasDirectoryList())
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
    if (!file_list_.HasDirectoryList())
        return 0;

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
    MoveToDrive(UiDriveList::kComputerObjectIndex);
    return 0;
}

} // namespace aspia
