//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_PANEL_H
#define _ASPIA_UI__FILE_MANAGER_PANEL_H

#include "ui/base/child_window.h"
#include "ui/base/listview.h"
#include "ui/base/imagelist.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileManagerPanel : public ChildWindow
{
public:
    FileManagerPanel() = default;
    virtual ~FileManagerPanel() = default;

    enum class Type { UNKNOWN, LOCAL, REMOTE };

    class Delegate
    {
    public:
        virtual void OnDriveListRequest(Type type) = 0;
    };

    bool CreatePanel(HWND parent, Type type, Delegate* delegate);

    void AddDriveItem(proto::DriveListItem::Type drive_type,
                      const std::wstring& drive_path,
                      const std::wstring& drive_name,
                      const std::wstring& drive_filesystem,
                      uint64_t total_space,
                      uint64_t free_space);

    void AddDirectoryItem(proto::DirectoryListItem::Type item_type,
                          const std::wstring& item_name,
                          uint64_t item_size);

private:
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnDrawItem(LPDRAWITEMSTRUCT dis);
    void OnGetDispInfo(LPNMHDR phdr);

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    Type type_ = Type::UNKNOWN;
    Delegate* delegate_ = nullptr;
    std::wstring name_;

    Window title_window_;

    ListView list_window_;

    Window toolbar_window_;
    ImageList toolbar_imagelist_;

    Window address_window_;
    Window status_window_;

    DISALLOW_COPY_AND_ASSIGN(FileManagerPanel);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_PANEL_H
