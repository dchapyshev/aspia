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

class UiModule
{
public:
    UiModule() = default;
    UiModule(const UiModule& other);
    ~UiModule() = default;

    static UiModule Current();

    UiModule& operator=(const UiModule& other);

    HINSTANCE Handle() const;

    std::wstring String(UINT resource_id) const;
    HICON Icon(UINT resource_id, int width, int height, UINT flags) const;
    HICON SmallIcon(UINT resource_id) const;
    HMENU Menu(UINT resource_id) const;

private:
    explicit UiModule(HINSTANCE instance);

    HINSTANCE instance_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__MODULE_H
