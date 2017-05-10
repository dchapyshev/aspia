//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/module.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__MODULE_H
#define _ASPIA_UI__BASE__MODULE_H

#include <string>

namespace aspia {

class Module
{
public:
    Module() = default;
    Module(const Module& other);
    ~Module() = default;

    static Module Current();

    Module& operator=(const Module& other);

    HINSTANCE Handle() const;

    std::wstring string(UINT resource_id) const;
    HICON icon(UINT resource_id, int width, int height, UINT flags) const;
    HMENU menu(UINT resource_id) const;

private:
    explicit Module(HINSTANCE instance);

    HINSTANCE instance_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__MODULE_H
