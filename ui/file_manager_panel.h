//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_PANEL_H
#define _ASPIA_UI__FILE_MANAGER_PANEL_H

#include "ui/base/child_window.h"
#include "ui/base/comboboxex.h"
#include "ui/base/listview.h"
#include "ui/base/imagelist.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class UiFileManagerPanel : public UiChildWindow
{
public:
    UiFileManagerPanel() = default;
    virtual ~UiFileManagerPanel() = default;

    enum class Type { UNKNOWN, LOCAL, REMOTE };

    class Delegate
    {
    public:
        virtual void OnDriveListRequest(Type type) = 0;
        virtual void OnDirectoryListRequest(Type type, const std::string& path) = 0;
    };

    bool CreatePanel(HWND parent, Type type, Delegate* delegate);

    void ReadDriveList(std::unique_ptr<proto::DriveList> drive_list);
    void ReadDirectoryList(std::unique_ptr<proto::DirectoryList> directory_list);

private:
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnDrawItem(LPDRAWITEMSTRUCT dis);
    void OnGetDispInfo(LPNMHDR phdr);
    void OnAddressChanged();
    void OnAddressChange(LPNMITEMACTIVATE item_activate);

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    Type type_ = Type::UNKNOWN;
    Delegate* delegate_ = nullptr;
    std::wstring name_;

    UiWindow title_window_;

    std::unique_ptr<proto::DirectoryList> directory_list_;
    UiListView list_window_;
    UiImageList list_imagelist_;

    UiWindow toolbar_window_;
    UiImageList toolbar_imagelist_;

    std::unique_ptr<proto::DriveList> drive_list_;
    UiComboBoxEx address_window_;
    UiImageList address_imagelist_;

    UiWindow status_window_;

    DISALLOW_COPY_AND_ASSIGN(UiFileManagerPanel);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_PANEL_H
