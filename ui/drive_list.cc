//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/drive_list.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/drive_list.h"
#include "ui/file_manager_helpers.h"
#include "ui/resource.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

bool UiDriveList::CreateDriveList(HWND parent, int control_id)
{
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        WS_VSCROLL | CBS_DROPDOWN;

    CRect drive_rect(0, 0, 200, 200);

    if (!Create(parent, drive_rect, nullptr, style, 0, control_id))
    {
        DLOG(ERROR) << "Unable to create drive list window: "
                    << GetLastSystemErrorString();
        return false;
    }

    return true;
}

void UiDriveList::Read(std::unique_ptr<proto::DriveList> list)
{
    ResetContent();
    imagelist_.RemoveAll();

    list_.reset(list.release());

    CIcon icon(GetComputerIcon());

    int icon_index = imagelist_.AddIcon(icon);

    CString text;
    text.LoadStringW(IDS_FT_COMPUTER);

    AddItem(text, icon_index, icon_index, 0, kComputerObjectIndex);

    const int object_count = list_->item_size();

    for (int object_index = 0; object_index < object_count; ++object_index)
    {
        const proto::DriveListItem& object = list_->item(object_index);

        icon = GetDriveIcon(object.type());
        icon_index = imagelist_.AddIcon(icon);

        AddItem(GetDriveDisplayName(object),
                icon_index,
                icon_index,
                1,
                object_index);
    }
}

bool UiDriveList::HasDriveList() const
{
    return list_ != nullptr;
}

bool UiDriveList::IsValidObjectIndex(int object_index) const
{
    if (!HasDriveList())
        return false;

    if (object_index >= 0 && object_index < list_->item_size())
        return true;

    return false;
}

void UiDriveList::SelectObject(int object_index)
{
    int item_index = GetItemIndexByObjectIndex(object_index);

    if (item_index != CB_ERR)
        SetCurSel(item_index);
}

int UiDriveList::SelectedObject() const
{
    if (!HasDriveList())
        return kInvalidObjectIndex;

    int selected_item = GetCurSel();
    if (selected_item == CB_ERR)
        return kInvalidObjectIndex;

    return GetItemData(selected_item);
}

const proto::DriveListItem& UiDriveList::Object(int object_index) const
{
    DCHECK(HasDriveList());
    DCHECK(IsValidObjectIndex(object_index));
    return list_->item(object_index);
}

const proto::DriveList& UiDriveList::DriveList() const
{
    DCHECK(HasDriveList());
    return *list_;
}

void UiDriveList::SetCurrentPath(const std::string& path)
{
    int current_folder_item_index =
        GetItemIndexByObjectIndex(kCurrentFolderObjectIndex);

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
                   UNICODEfromUTF8(path).c_str(),
                   icon_index,
                   icon_index,
                   0,
                   known_object_index);
    }

    SelectObject(known_object_index);
}

std::string UiDriveList::ObjectPath(int object_index) const
{
    switch (object_index)
    {
        case kCurrentFolderObjectIndex:
        {
            int item_index = GetItemIndexByObjectIndex(object_index);

            if (item_index == CB_ERR)
                return std::string();

            WCHAR path[MAX_PATH];

            if (!GetItemText(item_index, path, _countof(path)))
                return std::string();

            return UTF8fromUNICODE(path);
        }

        case kComputerObjectIndex:
        case kInvalidObjectIndex:
            return std::string();

        default:
            return list_->item(object_index).path();
    }
}

LRESULT UiDriveList::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LRESULT ret = DefWindowProcW(message, wparam, lparam);

    SetFont(AtlGetStockFont(DEFAULT_GUI_FONT));

    if (imagelist_.Create(GetSystemMetrics(SM_CXSMICON),
                          GetSystemMetrics(SM_CYSMICON),
                          ILC_MASK | ILC_COLOR32,
                          1, 1))
    {
        SetImageList(imagelist_);
    }

    return ret;
}

int UiDriveList::GetItemIndexByObjectIndex(int object_index) const
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

int UiDriveList::GetKnownObjectIndex(const std::string& path) const
{
    if (!HasDriveList())
        return kInvalidObjectIndex;

    if (path.empty())
        return kComputerObjectIndex;

    const int count = list_->item_size();

    for (int object_index = 0; object_index < count; ++object_index)
    {
        if (_stricmp(path.c_str(), list_->item(object_index).path().c_str()) == 0)
            return object_index;
    }

    return kCurrentFolderObjectIndex;
}

} // namespace aspia
