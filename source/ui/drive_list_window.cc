//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/drive_list_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/drive_list_window.h"
#include "ui/file_manager_helpers.h"
#include "ui/resource.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

namespace aspia {

bool DriveListWindow::CreateDriveList(HWND parent, int control_id)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        CBS_DROPDOWN;

    CRect drive_rect(0, 0, 200, 200);

    if (!Create(parent, drive_rect, nullptr, style, 0, control_id))
    {
        DLOG(ERROR) << "Unable to create drive list window: " << GetLastSystemErrorString();
        return false;
    }

    SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));

    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);
    }

    return true;
}

void DriveListWindow::Read(std::shared_ptr<proto::DriveList> list)
{
    ResetContent();
    imagelist_.RemoveAll();

    list_ = list;

    CIcon icon(GetComputerIcon());

    int icon_index = imagelist_.AddIcon(icon);

    CString text;
    text.LoadStringW(IDS_FT_COMPUTER);

    AddItem(text, icon_index, icon_index, 0, kComputerObjectIndex);

    const int object_count = list_->item_size();

    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DriveList::Item& object = list_->item(object_index);

        icon = GetDriveIcon(object.type());
        icon_index = imagelist_.AddIcon(icon);

        AddItem(GetDriveDisplayName(object),
                icon_index,
                icon_index,
                1,
                object_index);
    }
}

bool DriveListWindow::HasDriveList() const
{
    return list_ != nullptr;
}

bool DriveListWindow::IsValidObjectIndex(int object_index) const
{
    if (!HasDriveList())
        return false;

    if (object_index >= 0 && object_index < list_->item_size())
        return true;

    return false;
}

void DriveListWindow::SelectObject(int object_index)
{
    int item_index = GetItemIndexByObjectIndex(object_index);

    if (item_index != CB_ERR)
        SetCurSel(item_index);
}

int DriveListWindow::SelectedObject() const
{
    if (!HasDriveList())
        return kInvalidObjectIndex;

    int selected_item = GetCurSel();
    if (selected_item == CB_ERR)
        return kInvalidObjectIndex;

    return GetItemData(selected_item);
}

const proto::DriveList::Item& DriveListWindow::Object(int object_index) const
{
    DCHECK(HasDriveList());
    DCHECK(IsValidObjectIndex(object_index));
    return list_->item(object_index);
}

const proto::DriveList& DriveListWindow::DriveList() const
{
    DCHECK(HasDriveList());
    return *list_;
}

void DriveListWindow::SetCurrentPath(const FilePath& path)
{
    current_path_ = path;

    int current_folder_item_index = GetItemIndexByObjectIndex(kCurrentFolderObjectIndex);

    if (current_folder_item_index != CB_ERR)
    {
        COMBOBOXEXITEMW item;
        memset(&item, 0, sizeof(item));

        item.mask = CBEIF_IMAGE;
        item.iItem = current_folder_item_index;

        if (GetItem(&item))
            imagelist_.Remove(item.iImage);

        DeleteItem(current_folder_item_index);
    }

    int known_object_index = GetKnownObjectIndex(path);
    if (known_object_index == kCurrentFolderObjectIndex)
    {
        // All directories have the same icon.
        CIcon icon(GetDirectoryIcon());
        int icon_index = imagelist_.AddIcon(icon);

        InsertItem(0,
                   path.c_str(),
                   icon_index,
                   icon_index,
                   0,
                   known_object_index);
    }

    SelectObject(known_object_index);
}

const FilePath& DriveListWindow::CurrentPath() const
{
    return current_path_;
}

FilePath DriveListWindow::ObjectPath(int object_index) const
{
    switch (object_index)
    {
        case kCurrentFolderObjectIndex:
        {
            int item_index = GetItemIndexByObjectIndex(object_index);

            if (item_index == CB_ERR)
                return FilePath();

            WCHAR path[MAX_PATH];

            if (!GetItemText(item_index, path, _countof(path)))
                return FilePath();

            return path;
        }

        case kComputerObjectIndex:
        case kInvalidObjectIndex:
            return FilePath();

        default:
            return std::experimental::filesystem::u8path(list_->item(object_index).path());
    }
}

int DriveListWindow::GetItemIndexByObjectIndex(int object_index) const
{
    DWORD_PTR data = static_cast<DWORD_PTR>(object_index);
    int item_count = GetCount();

    for (int item_index = 0; item_index < item_count; ++item_index)
    {
        if (GetItemData(item_index) == data)
            return item_index;
    }

    return CB_ERR;
}

int DriveListWindow::GetKnownObjectIndex(const FilePath& path) const
{
    if (!HasDriveList())
        return kInvalidObjectIndex;

    if (path.empty())
        return kComputerObjectIndex;

    const int count = list_->item_size();

    for (int object_index = 0; object_index < count; ++object_index)
    {
        FilePath known_path =
            std::experimental::filesystem::u8path(list_->item(object_index).path());

        if (CompareCaseInsensitive(known_path, path) == 0)
            return object_index;
    }

    return kCurrentFolderObjectIndex;
}

} // namespace aspia
