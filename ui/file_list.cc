//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_list.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_list.h"
#include "ui/file_manager_helpers.h"
#include "ui/resource.h"
#include "base/version_helpers.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

#include <uxtheme.h>

namespace aspia {

void UiFileList::Read(std::unique_ptr<proto::DirectoryList> list)
{
    DeleteAllItems();
    imagelist_.RemoveAll();
    DeleteAllColumns();

    ModifyStyle(LVS_SINGLESEL, LVS_EDITLABELS);

    AddNewColumn(IDS_FT_COLUMN_NAME, 180);
    AddNewColumn(IDS_FT_COLUMN_SIZE, 70);
    AddNewColumn(IDS_FT_COLUMN_TYPE, 100);
    AddNewColumn(IDS_FT_COLUMN_MODIFIED, 100);

    list_.reset(list.release());

    // All directories have the same icon.
    CIcon icon(GetDirectoryIcon());

    int icon_index = imagelist_.AddIcon(icon);
    int object_count = list_->item_size();

    // Enumerate the directories first.
    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DirectoryListItem& item = list_->item(object_index);

        if (item.type() != proto::DirectoryListItem::DIRECTORY)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        int item_index = AddItem(GetItemCount(), 0, name.c_str(), icon_index);
        SetItemData(item_index, object_index);
        SetItemText(item_index, 2, GetDirectoryTypeString(name));
        SetItemText(item_index, 3, TimeToString(item.modified()).c_str());
    }

    // Enumerate the files.
    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DirectoryListItem& item = list_->item(object_index);

        if (item.type() != proto::DirectoryListItem::FILE)
            continue;

        std::wstring name = UNICODEfromUTF8(item.name());

        icon = GetFileIcon(name);
        icon_index = imagelist_.AddIcon(icon);

        int item_index = AddItem(GetItemCount(), 0, name.c_str(), icon_index);

        SetItemData(item_index, object_index);
        SetItemText(item_index, 1, SizeToString(item.size()).c_str());
        SetItemText(item_index, 2, GetFileTypeString(name));
        SetItemText(item_index, 3, TimeToString(item.modified()).c_str());
    }
}

void UiFileList::Read(const proto::DriveList& list)
{
    DeleteAllItems();
    imagelist_.RemoveAll();
    DeleteAllColumns();

    ModifyStyle(LVS_EDITLABELS, LVS_SINGLESEL);

    AddNewColumn(IDS_FT_COLUMN_NAME, 130);
    AddNewColumn(IDS_FT_COLUMN_TYPE, 150);
    AddNewColumn(IDS_FT_COLUMN_TOTAL_SPACE, 80);
    AddNewColumn(IDS_FT_COLUMN_FREE_SPACE, 80);

    list_.reset();

    const int object_count = list.item_size();

    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DriveListItem& object = list.item(object_index);

        CIcon icon(GetDriveIcon(object.type()));
        int icon_index = imagelist_.AddIcon(icon);

        CString display_name(GetDriveDisplayName(object));

        int item_index = AddItem(GetItemCount(), 0, display_name, icon_index);
        SetItemData(item_index, object_index);
        SetItemText(item_index, 1, GetDriveDescription(object.type()));

        if (object.total_space())
            SetItemText(item_index, 2, SizeToString(object.total_space()).c_str());

        if (object.free_space())
            SetItemText(item_index, 3, SizeToString(object.free_space()).c_str());
    }
}

bool UiFileList::HasDirectoryList() const
{
    return list_ != nullptr;
}

bool UiFileList::HasParentDirectory() const
{
    if (!HasDirectoryList())
        return false;

    return list_->has_parent();
}

const std::string& UiFileList::CurrentPath() const
{
    DCHECK(HasDirectoryList());
    return list_->path();
}

const proto::DirectoryListItem& UiFileList::Object(int object_index)
{
    DCHECK(HasDirectoryList());
    DCHECK(IsValidObjectIndex(object_index));
    return list_->item(object_index);
}

const std::string& UiFileList::ObjectName(int object_index)
{
    DCHECK(HasDirectoryList());
    DCHECK(IsValidObjectIndex(object_index));
    return list_->item(object_index).name();
}

proto::DirectoryListItem* UiFileList::FirstSelectedObject() const
{
    int selected_item = GetNextItem(-1, LVNI_SELECTED);
    if (selected_item == -1)
        return nullptr;

    int object_index = GetItemData(selected_item);

    if (!IsValidObjectIndex(object_index))
        return nullptr;

    return list_->mutable_item(object_index);
}

void UiFileList::AddDirectory()
{
    if (!HasDirectoryList())
        return;

    CIcon folder_icon(GetDirectoryIcon());
    int icon_index = imagelist_.AddIcon(folder_icon);

    CString folder_name;
    folder_name.LoadStringW(IDS_FT_NEW_FOLDER);

    SetFocus();

    int item_index = AddItem(GetItemCount(),
                             0,
                             folder_name,
                             icon_index);

    SetItemData(item_index, kNewFolderObjectIndex);
    EditLabel(item_index);
}

LRESULT UiFileList::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        ::SetWindowTheme(*this, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    SetExtendedListViewStyle(ex_style);

    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_, LVSIL_SMALL);
    }

    return ret;
}

int UiFileList::GetColumnCount() const
{
    CHeaderCtrl header(GetHeader());

    if (!header)
        return 0;

    return header.GetItemCount();
}

void UiFileList::DeleteAllColumns()
{
    int count = GetColumnCount();

    while (--count >= 0)
        DeleteColumn(count);
}

void UiFileList::AddNewColumn(UINT string_id, int width)
{
    CString text;
    text.LoadStringW(string_id);

    int column_index = AddColumn(text, GetColumnCount());
    SetColumnWidth(column_index, width);
}

bool UiFileList::IsValidObjectIndex(int object_index) const
{
    if (!list_)
        return false;

    if (object_index >= 0 && object_index < list_->item_size())
        return true;

    return false;
}

UiFileList::Iterator::Iterator(const UiFileList& list, Mode mode) :
    list_(list),
    mode_(mode)
{
    item_index_ = list.GetNextItem(-1, mode_);
}

bool UiFileList::Iterator::IsAtEnd() const
{
    return item_index_ == -1;
}

void UiFileList::Iterator::Advance()
{
    item_index_ = list_.GetNextItem(item_index_, mode_);
}

proto::DirectoryListItem* UiFileList::Iterator::Object() const
{
    if (item_index_ == -1)
        return nullptr;

    int object_index = list_.GetItemData(item_index_);

    if (!list_.IsValidObjectIndex(object_index))
        return nullptr;

    return list_.list_->mutable_item(object_index);
}

} // namespace aspia
