//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/splitter.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SPLITTER_H
#define _ASPIA_UI__BASE__SPLITTER_H

#include "ui/base/child_window.h"

namespace aspia {

class Splitter : public ChildWindow
{
public:
    Splitter() = default;
    ~Splitter() = default;

    bool CreateWithFixedLeft(HWND parent, int position);
    bool CreateWithProportion(HWND parent);
    void SetPanels(HWND first, HWND second);

private:
    bool CreateInternal(HWND parent);
    void OnCreate();
    void OnDestroy();
    void OnLButtonDown();
    void OnLButtonUp();
    void OnMouseMove();
    void OnSize(const DesktopSize& size);
    void OnDrawItem(LPDRAWITEMSTRUCT dis);
    void Draw();
    int NormalizePosition(int position);

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    bool fixed_left_ = true;

    HCURSOR cursor_;

    Window left_panel_;
    Window right_panel_;
    Window split_panel_;

    DesktopSize prev_size_;

    bool is_sizing_ = false;

    int x_ = 0;
    int height_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Splitter);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__SPLITTER_H
