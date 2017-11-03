//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer/file_manager_panel.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_transfer/file_manager_panel.h"
#include "ui/file_transfer/file_remove_dialog.h"
#include "ui/file_transfer/file_manager.h"
#include "ui/file_transfer/file_manager_helpers.h"
#include "ui/status_code.h"
#include "base/logging.h"

#include <uxtheme.h>

namespace aspia {

static_assert(FileListCtrl::kInvalidObjectIndex == DriveListCtrl::kInvalidObjectIndex,
              "Values must be equal");

FileManagerPanel::FileManagerPanel(Type type,
                                   std::shared_ptr<FileRequestSenderProxy> sender,
                                   Delegate* delegate)
    : toolbar_(type == Type::LOCAL ?
          FileToolBar::Type::LOCAL : FileToolBar::Type::REMOTE),
      type_(type),
      sender_(sender),
      delegate_(delegate)
{
    // Nothing
}

FilePath FileManagerPanel::GetCurrentPath() const
{
    return drive_list_.CurrentPath();
}

void FileManagerPanel::Refresh()
{
    sender_->SendDriveListRequest(This());
}

LPARAM FileManagerPanel::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    HFONT default_font = AtlGetDefaultGuiFont();

    CString panel_name;

    if (type_ == Type::LOCAL)
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
    status_.Create(*this, status_rect, nullptr, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW);
    status_.SetFont(default_font);

    sender_->SendDriveListRequest(This());
    return 0;
}

LRESULT FileManagerPanel::OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    drive_list_.DestroyWindow();
    file_list_.DestroyWindow();
    toolbar_.DestroyWindow();
    title_.DestroyWindow();
    status_.DestroyWindow();
    return 0;
}

LRESULT FileManagerPanel::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(handled);

    HDWP dwp = BeginDeferWindowPos(5);

    if (!dwp)
        return 0;

    CSize size(static_cast<DWORD>(lparam));

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
                          0, 0,
                          size.cx, title_rect.Height(),
                          SWP_NOACTIVATE | SWP_NOZORDER);

    drive_list_.DeferWindowPos(dwp, nullptr,
                               0, title_rect.Height(),
                               size.cx, drive_rect.Height(),
                               SWP_NOACTIVATE | SWP_NOZORDER);

    toolbar_.DeferWindowPos(dwp, nullptr,
                            0, title_rect.Height() + drive_rect.Height(),
                            size.cx, toolbar_rect.Height(),
                            SWP_NOACTIVATE | SWP_NOZORDER);

    int list_y = title_rect.Height() + toolbar_rect.Height() + drive_rect.Height();
    int list_height = size.cy - list_y - status_rect.Height();

    file_list_.DeferWindowPos(dwp, nullptr,
                              0, list_y,
                              size.cx, list_height,
                              SWP_NOACTIVATE | SWP_NOZORDER);

    status_.DeferWindowPos(dwp, nullptr,
                           0, list_y + list_height,
                           size.cx, status_rect.Height(),
                           SWP_NOACTIVATE | SWP_NOZORDER);

    EndDeferWindowPos(dwp);

    return 0;
}

LRESULT FileManagerPanel::OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(handled);

    LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lparam);

    if (dis->hwndItem != title_ && dis->hwndItem != status_)
        return 0;

    int saved_dc = SaveDC(dis->hDC);
    if (!saved_dc)
        return 0;

    // Transparent background.
    SetBkMode(dis->hDC, TRANSPARENT);

    HBRUSH background_brush = GetSysColorBrush(COLOR_WINDOW);
    FillRect(dis->hDC, &dis->rcItem, background_brush);

    WCHAR label[256] = { 0 };
    ::GetWindowTextW(dis->hwndItem, label, _countof(label));

    if (label[0])
    {
        SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
        DrawTextW(dis->hDC, label, -1, &dis->rcItem, DT_VCENTER | DT_SINGLELINE);
    }

    RestoreDC(dis->hDC, saved_dc);

    return 0;
}

LRESULT FileManagerPanel::OnDriveChange(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    int object_index = drive_list_.SelectedObject();

    if (object_index == DriveListCtrl::kInvalidObjectIndex)
        return 0;

    MoveToDrive(object_index);
    return 0;
}

LRESULT FileManagerPanel::OnListDoubleClock(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(hdr);
    UNUSED_PARAMETER(handled);

    int object_index = file_list_.GetObjectUnderMousePointer();

    if (object_index == FileListCtrl::kInvalidObjectIndex)
        return 0;

    if (!file_list_.HasFileList())
    {
        MoveToDrive(object_index);
        return 0;
    }

    if (!file_list_.IsValidObjectIndex(object_index))
        return 0;

    if (file_list_.IsDirectoryObject(object_index))
    {
        FilePath path(drive_list_.CurrentPath());
        path.append(file_list_.ObjectName(object_index));
        sender_->SendFileListRequest(This(), path);
    }

    return 0;
}

void FileManagerPanel::FolderUp()
{
    FilePath path = drive_list_.CurrentPath();

    if (!path.has_parent_path() || path.parent_path() == path.root_name())
    {
        MoveToDrive(DriveListCtrl::kComputerObjectIndex);
    }
    else
    {
        sender_->SendFileListRequest(This(), path.parent_path());
    }
}

LRESULT FileManagerPanel::OnFolderUp(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    FolderUp();
    return 0;
}

LRESULT FileManagerPanel::OnFolderAdd(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    file_list_.AddDirectory();
    return 0;
}

LRESULT FileManagerPanel::OnRefresh(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    Refresh();
    return 0;
}

void FileManagerPanel::RemoveSelectedFiles()
{
    if (!file_list_.HasFileList())
        return;

    UINT selected_count = file_list_.GetSelectedCount();
    if (!selected_count)
        return;

    CString title;
    title.LoadStringW(IDS_CONFIRMATION);

    CString message;
    message.Format(IDS_FT_DELETE_CONFORM, selected_count);

    if (MessageBoxW(message, title, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    FileTaskQueueBuilder::FileList file_list;

    // Create a list of files and directories to remove.
    for (FileListCtrl::Iterator iter(file_list_, FileListCtrl::Iterator::SELECTED);
         !iter.IsAtEnd();
         iter.Advance())
    {
        file_list.emplace_back(iter.Object());
    }

    // If the list is empty (there are no selected items).
    if (file_list.empty())
        return;

    FileRemoveDialog dialog(sender_, drive_list_.CurrentPath(), file_list);
    dialog.DoModal(*this);

    Refresh();
}

LRESULT FileManagerPanel::OnRemove(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    RemoveSelectedFiles();
    return 0;
}

void FileManagerPanel::MoveToDrive(int object_index)
{
    if (object_index == DriveListCtrl::kComputerObjectIndex)
    {
        toolbar_.EnableButton(ID_FOLDER_ADD, FALSE);
        toolbar_.EnableButton(ID_FOLDER_UP, FALSE);
        toolbar_.EnableButton(ID_DELETE, FALSE);
        toolbar_.EnableButton(ID_SEND, FALSE);
        toolbar_.EnableButton(ID_HOME, FALSE);

        file_list_.Read(drive_list_.DriveList());
        drive_list_.SetCurrentPath(FilePath());
    }
    else
    {
        sender_->SendFileListRequest(This(), drive_list_.ObjectPath(object_index));
    }
}

LRESULT FileManagerPanel::OnListEndLabelEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(handled);

    LPNMLVDISPINFOW disp_info = reinterpret_cast<LPNMLVDISPINFOW>(hdr);

    if (!file_list_.HasFileList())
        return 0;

    int object_index = static_cast<int>(disp_info->item.lParam);

    // New folder.
    if (object_index == FileListCtrl::kNewFolderObjectIndex)
    {
        CEdit edit(file_list_.GetEditControl());

        WCHAR buffer[MAX_PATH] = { 0 };
        edit.GetWindowTextW(buffer, _countof(buffer));

        FilePath path = drive_list_.CurrentPath();
        path.append(buffer);

        sender_->SendCreateDirectoryRequest(This(), path);
    }
    else // Rename exists item.
    {
        DCHECK(file_list_.IsValidObjectIndex(object_index));

        // User canceled rename.
        if (!disp_info->item.pszText)
            return 0;

        FilePath old_name = drive_list_.CurrentPath();
        old_name.append(file_list_.ObjectName(object_index));

        FilePath new_name = drive_list_.CurrentPath();
        new_name.append(disp_info->item.pszText);

        sender_->SendRenameRequest(This(), old_name, new_name);
    }

    return 0;
}

LRESULT FileManagerPanel::OnListItemChanged(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(hdr);
    UNUSED_PARAMETER(handled);

    UINT count = file_list_.GetSelectedCount();

    if (file_list_.HasFileList())
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

LRESULT FileManagerPanel::OnListBeginDrag(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(handled);

    if (!file_list_.HasFileList())
        return 0;

    int first_selected_item = file_list_.GetNextItem(-1, LVNI_SELECTED);
    if (first_selected_item == -1)
        return 0;

    CPoint pt;
    drag_imagelist_ = file_list_.CreateDragImage(first_selected_item, &pt);
    if (drag_imagelist_.IsNull())
        return 0;

    if (drag_imagelist_.BeginDrag(0, 0, 0))
    {
        pt = reinterpret_cast<LPNMLISTVIEW>(hdr)->ptAction;
        ClientToScreen(&pt);

        if (drag_imagelist_.DragEnter(GetDesktopWindow(), pt))
        {
            dragging_ = true;
            SetCapture();
            return 0;
        }

        drag_imagelist_.EndDrag();
    }

    drag_imagelist_.Destroy();
    return 0;
}

LRESULT FileManagerPanel::OnMouseMove(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(handled);

    if (!dragging_)
        return 0;

    CPoint pt(lparam);
    ClientToScreen(&pt);

    drag_imagelist_.DragMove(pt);
    return 0;
}

LRESULT FileManagerPanel::OnLButtonUp(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(handled);

    if (!dragging_)
        return 0;

    dragging_ = false;

    drag_imagelist_.DragLeave(file_list_);
    drag_imagelist_.EndDrag();
    drag_imagelist_.Destroy();

    ReleaseCapture();

    CPoint pt(lparam);
    ClientToScreen(&pt);

    Type type;

    if (delegate_->GetPanelTypeInPoint(pt, type) && type != type_)
    {
        SendSelectedFiles();
    }

    return 0;
}

void FileManagerPanel::SendSelectedFiles()
{
    FilePath source_path = drive_list_.CurrentPath();

    // If the path is empty, the panel displays a list of drives.
    if (source_path.empty())
        return;

    FileTaskQueueBuilder::FileList file_list;

    // Create a list of files and directories to copy.
    for (FileListCtrl::Iterator iter(file_list_, FileListCtrl::Iterator::SELECTED);
         !iter.IsAtEnd();
         iter.Advance())
    {
        file_list.emplace_back(iter.Object());
    }

    // If the list is empty (there are no selected items).
    if (file_list.empty())
        return;

    // Sending files. After the method is completed, the operation must be completed.
    delegate_->SendFiles(type_, drive_list_.CurrentPath(), file_list);
}

LRESULT FileManagerPanel::OnSend(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    SendSelectedFiles();
    return 0;
}

// static
int CALLBACK FileManagerPanel::CompareFunc(LPARAM lparam1, LPARAM lparam2, LPARAM lparam_sort)
{
    SortContext* context = reinterpret_cast<SortContext*>(lparam_sort);
    const FileListCtrl& list = context->self->file_list_;

    LVFINDINFOW find_info;
    find_info.flags = LVFI_PARAM;

    find_info.lParam = lparam1;
    int index = list.FindItem(&find_info, -1);

    WCHAR item1[MAX_PATH] = { 0 };
    list.GetItemText(index, context->column_index, item1, _countof(item1));

    find_info.lParam = lparam2;
    index = list.FindItem(&find_info, -1);

    WCHAR item2[MAX_PATH] = { 0 };
    list.GetItemText(index, context->column_index, item2, _countof(item2));

    if (context->self->sort_ascending_)
        return wcscmp(item2, item1);
    else
        return wcscmp(item1, item2);
}

LRESULT FileManagerPanel::OnListColumnClick(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(handled);

    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(hdr);

    SortContext context;
    context.self = this;
    context.column_index = pnmv->iSubItem;

    file_list_.SortItems(CompareFunc, reinterpret_cast<LPARAM>(&context));
    sort_ascending_ = !sort_ascending_;

    return 0;
}

LRESULT FileManagerPanel::OnDriveEndEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(handled);

    PNMCBEENDEDITW end_edit = reinterpret_cast<PNMCBEENDEDITW>(hdr);

    if (end_edit->fChanged && end_edit->iWhy == CBENF_RETURN && end_edit->szText[0])
    {
        sender_->SendFileListRequest(This(), FilePath(end_edit->szText));
    }

    return 0;
}

void FileManagerPanel::Home()
{
    MoveToDrive(DriveListCtrl::kComputerObjectIndex);
}

LRESULT FileManagerPanel::OnHome(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled)
{
    UNUSED_PARAMETER(code);
    UNUSED_PARAMETER(ctrl_id);
    UNUSED_PARAMETER(ctrl);
    UNUSED_PARAMETER(handled);

    Home();
    return 0;
}

void FileManagerPanel::OnDriveListReply(std::shared_ptr<proto::DriveList> drive_list,
                                        proto::RequestStatus status)
{
    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        CString status_string = RequestStatusCodeToString(status);

        CString message;
        message.Format(IDS_FT_OP_BROWSE_DRIVES_ERROR, status_string.GetBuffer(0));
        MessageBoxW(message, nullptr, MB_ICONWARNING | MB_OK);
    }
    else
    {
        drive_list_.Read(drive_list);

        if (file_list_.HasFileList())
        {
            sender_->SendFileListRequest(This(), GetCurrentPath());
        }
        else
        {
            MoveToDrive(DriveListCtrl::kComputerObjectIndex);
        }
    }
}

void FileManagerPanel::OnFileListReply(const FilePath& path,
                                       std::shared_ptr<proto::FileList> file_list,
                                       proto::RequestStatus status)
{
    if (status != proto::REQUEST_STATUS_SUCCESS)
    {
        CString status_string = RequestStatusCodeToString(status);

        CString message;
        message.Format(IDS_FT_BROWSE_FOLDERS_ERROR, path.c_str(), status_string.GetBuffer(0));
        MessageBoxW(message, nullptr, MB_ICONWARNING | MB_OK);
    }
    else
    {
        toolbar_.EnableButton(ID_FOLDER_ADD, TRUE);
        toolbar_.EnableButton(ID_FOLDER_UP, TRUE);
        toolbar_.EnableButton(ID_HOME, TRUE);

        file_list_.Read(file_list);
        drive_list_.SetCurrentPath(path);
    }
}

void FileManagerPanel::OnCreateDirectoryReply(const FilePath& path, proto::RequestStatus status)
{
    if (status != proto::RequestStatus::REQUEST_STATUS_SUCCESS)
    {
        CString status_string = RequestStatusCodeToString(status);

        CString message;
        message.Format(IDS_FT_OP_CREATE_FOLDER_ERROR, path.c_str(), status_string.GetBuffer(0));
        MessageBoxW(message, nullptr, MB_ICONWARNING | MB_OK);
    }
    else
    {
        Refresh();
    }
}

void FileManagerPanel::OnRenameReply(const FilePath& old_name,
                                     const FilePath& new_name,
                                     proto::RequestStatus status)
{
    if (status != proto::RequestStatus::REQUEST_STATUS_SUCCESS)
    {
        CString status_string = RequestStatusCodeToString(status);

        CString message;
        message.Format(IDS_FT_OP_RENAME_ERROR,
                       old_name.c_str(),
                       new_name.c_str(),
                       status_string.GetBuffer(0));
        MessageBoxW(message, nullptr, MB_ICONWARNING | MB_OK);
    }
    else
    {
        Refresh();
    }
}

} // namespace aspia
