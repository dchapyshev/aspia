//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/drive_list_ctrl.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__DRIVE_LIST_CTRL_H
#define _ASPIA_UI__FILE_TRANSFER__DRIVE_LIST_CTRL_H

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

#include <experimental/filesystem>
#include <memory>

namespace aspia {

class DriveListCtrl : public CWindowImpl<DriveListCtrl, CComboBoxEx>
{
public:
    static const int kInvalidObjectIndex = -1;
    static const int kComputerObjectIndex = -2;
    static const int kCurrentFolderObjectIndex = -3;

    DriveListCtrl() = default;
    ~DriveListCtrl() = default;

    bool CreateDriveList(HWND parent, int control_id);

    void Read(std::shared_ptr<proto::file_transfer::DriveList> list);

    bool HasDriveList() const;
    bool IsValidObjectIndex(int object_index) const;
    void SelectObject(int object_index);
    int SelectedObject() const;
    const proto::file_transfer::DriveList::Item& Object(int object_index) const;
    const proto::file_transfer::DriveList& DriveList() const;
    void SetCurrentPath(const std::experimental::filesystem::path& path);
    const std::experimental::filesystem::path& CurrentPath() const;
    std::experimental::filesystem::path ObjectPath(int object_index) const;

private:
    BEGIN_MSG_MAP(DriveListCtrl)
        // Nothing
    END_MSG_MAP()

    int GetItemIndexByObjectIndex(int object_index) const;
    int GetKnownObjectIndex(const std::experimental::filesystem::path& path) const;

    std::shared_ptr<proto::file_transfer::DriveList> list_;
    CImageListManaged imagelist_;
    std::experimental::filesystem::path current_path_;

    DISALLOW_COPY_AND_ASSIGN(DriveListCtrl);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__DRIVE_LIST_CTRL_H
