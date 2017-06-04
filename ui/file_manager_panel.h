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

namespace aspia {

class FileManagerPanel : public ChildWindow
{
public:
    FileManagerPanel() { }
    ~FileManagerPanel() { }

    enum class Type { UNKNOWN, LOCAL, REMOTE };

    bool CreatePanel(HWND parent, Type);

private:
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnDrawItem(LPDRAWITEMSTRUCT dis);
    void OnGetDispInfo(LPNMHDR phdr);

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    Type type_ = Type::UNKNOWN;
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
