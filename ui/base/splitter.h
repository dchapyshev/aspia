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
    Splitter() { }
    ~Splitter() { }

    void Create(HWND parent, int position, int min_first, int min_second);
    void SetPanels(HWND first, HWND second);

private:
    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    HWND first_;
    HWND second_;

    int position_ = 100;
    int min_first_ = 100;
    int min_second_ = 100;

    DISALLOW_COPY_AND_ASSIGN(Splitter);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__SPLITTER_H
