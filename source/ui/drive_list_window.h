//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/drive_list_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__DRIVE_LIST_WINDOW_H
#define _ASPIA_UI__DRIVE_LIST_WINDOW_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <memory>

namespace aspia {

class DriveListWindow : public CWindowImpl<DriveListWindow, CComboBoxEx>
{
public:
    static const int kInvalidObjectIndex = -1;
    static const int kComputerObjectIndex = -2;
    static const int kCurrentFolderObjectIndex = -3;

    DriveListWindow() = default;
    ~DriveListWindow() = default;

    bool CreateDriveList(HWND parent, int control_id);

    void Read(std::unique_ptr<proto::DriveList> list);

    bool HasDriveList() const;
    bool IsValidObjectIndex(int object_index) const;
    void SelectObject(int object_index);
    int SelectedObject() const;
    const proto::DriveList::Item& Object(int object_index) const;
    const proto::DriveList& DriveList() const;
    void SetCurrentPath(const FilePath& path);
    const FilePath& CurrentPath() const;
    FilePath ObjectPath(int object_index) const;

private:
    BEGIN_MSG_MAP(DriveListWindow)
        // Nothing
    END_MSG_MAP()

    int GetItemIndexByObjectIndex(int object_index) const;
    int GetKnownObjectIndex(const FilePath& path) const;

    std::unique_ptr<proto::DriveList> list_;
    CImageListManaged imagelist_;
    FilePath current_path_;

    DISALLOW_COPY_AND_ASSIGN(DriveListWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__DRIVE_LIST_WINDOW_H
