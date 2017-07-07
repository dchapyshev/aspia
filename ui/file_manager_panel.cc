//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.cc
// LICENSE:         Mozilla Public License Version 2.0
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

namespace fs = std::experimental::filesystem;

static_assert(UiFileList::kInvalidObjectIndex == UiDriveList::kInvalidObjectIndex,
              "Values must be equal");

UiFileManagerPanel::UiFileManagerPanel(PanelType panel_type,
                                       std::shared_ptr<FileRequestSenderProxy> sender) :
    toolbar_(panel_type == PanelType::LOCAL ?
             UiFileToolBar::Type::LOCAL : UiFileToolBar::Type::REMOTE),
    panel_type_(panel_type),
    sender_(sender)
{
    // Nothing
}

LPARAM UiFileManagerPanel::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
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

    sender_->SendDriveListRequest(This());
    return 0;
}

LRESULT UiFileManagerPanel::OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    drive_list_.DestroyWindow();
    file_list_.DestroyWindow();
    toolbar_.DestroyWindow();
    title_.DestroyWindow();
    status_.DestroyWindow();
    return 0;
}

LRESULT UiFileManagerPanel::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
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

LRESULT UiFileManagerPanel::OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
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

LRESULT UiFileManagerPanel::OnDriveChange(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    int object_index = drive_list_.SelectedObject();

    if (object_index == UiDriveList::kInvalidObjectIndex)
        return 0;

    MoveToDrive(object_index);
    return 0;
}

LRESULT UiFileManagerPanel::OnListDoubleClock(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    int object_index = file_list_.GetObjectUnderMousePointer();

    if (!file_list_.HasDirectoryList())
    {
        MoveToDrive(object_index);
        return 0;
    }

    if (!file_list_.IsValidObjectIndex(object_index))
        return 0;

    const proto::FileList::Item& item = file_list_.Object(object_index);

    if (file_list_.IsDirectoryObject(object_index))
    {
        FilePath path(file_list_.CurrentPath());
        path.append(file_list_.ObjectName(object_index));

        sender_->SendFileListRequest(This(), path);
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderUp(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    FilePath path = file_list_.CurrentPath();

    if (!path.has_parent_path() || path.parent_path() == path.root_name())
    {
        MoveToDrive(UiDriveList::kComputerObjectIndex);
    }
    else
    {
        sender_->SendFileListRequest(This(), path.parent_path());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnFolderAdd(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    file_list_.AddDirectory();
    return 0;
}

LRESULT UiFileManagerPanel::OnRefresh(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    sender_->SendDriveListRequest(This());

    if (file_list_.HasDirectoryList())
    {
        sender_->SendFileListRequest(This(), file_list_.CurrentPath());
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnRemove(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
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
        proto::FileList::Item* object = file_list_.FirstSelectedObject();
        if (!object)
            return 0;

        object_list = UNICODEfromUTF8(object->name());

        if (object->is_directory())
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
            proto::FileList::Item* object = iter.Object();
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
            proto::FileList::Item* object = iter.Object();
            if (!object)
                continue;

            FilePath path = file_list_.CurrentPath();
            path.append(fs::u8path(object->name()));

            sender_->SendRemoveRequest(This(), path);
        }
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
        sender_->SendFileListRequest(This(), drive_list_.ObjectPath(object_index));
    }
}

LRESULT UiFileManagerPanel::OnListEndLabelEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled)
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

        FilePath path = file_list_.CurrentPath();
        path.append(buffer);

        sender_->SendCreateDirectoryRequest(This(), path);
    }
    else // Rename exists item.
    {
        DCHECK(file_list_.IsValidObjectIndex(object_index));

        // User canceled rename.
        if (!disp_info->item.pszText)
            return 0;

        FilePath old_name = file_list_.CurrentPath();
        old_name.append(file_list_.ObjectName(object_index));

        FilePath new_name = file_list_.CurrentPath();
        new_name.append(disp_info->item.pszText);

        sender_->SendRenameRequest(This(), old_name, new_name);
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnListItemChanged(int ctrl_id, LPNMHDR hdr, BOOL& handled)
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

LRESULT UiFileManagerPanel::OnSend(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    if (!file_list_.HasDirectoryList())
        return 0;

    return 0;
}

LRESULT UiFileManagerPanel::OnDriveEndEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    PNMCBEENDEDITW end_edit = reinterpret_cast<PNMCBEENDEDITW>(hdr);

    if (end_edit->fChanged && end_edit->iWhy == CBENF_RETURN && end_edit->szText)
    {
        sender_->SendFileListRequest(This(), FilePath(end_edit->szText));
    }

    return 0;
}

LRESULT UiFileManagerPanel::OnHome(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    MoveToDrive(UiDriveList::kComputerObjectIndex);
    return 0;
}

void UiFileManagerPanel::OnLastRequestFailed(proto::Status status)
{

}

void UiFileManagerPanel::OnDriveListReply(std::unique_ptr<proto::DriveList> drive_list)
{
    drive_list_.Read(std::move(drive_list));

    if (!file_list_.HasDirectoryList())
    {
        MoveToDrive(UiDriveList::kComputerObjectIndex);
    }
}

void UiFileManagerPanel::OnFileListReply(std::unique_ptr<proto::FileList> file_list)
{
    toolbar_.EnableButton(ID_FOLDER_ADD, TRUE);
    toolbar_.EnableButton(ID_FOLDER_UP, TRUE);
    toolbar_.EnableButton(ID_HOME, TRUE);

    file_list_.Read(std::move(file_list));
    drive_list_.SetCurrentPath(file_list_.CurrentPath());
}

void UiFileManagerPanel::OnCreateDirectoryReply()
{
    sender_->SendFileListRequest(This(), file_list_.CurrentPath());
}

void UiFileManagerPanel::OnRemoveReply()
{
    sender_->SendFileListRequest(This(), file_list_.CurrentPath());
}

void UiFileManagerPanel::OnRenameReply()
{
    sender_->SendFileListRequest(This(), file_list_.CurrentPath());
}

} // namespace aspia
