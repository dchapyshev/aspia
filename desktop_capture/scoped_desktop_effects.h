/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/scoped_desktop_effects.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__SCOPED_DESKTOP_EFFECTS_H
#define _ASPIA_DESKTOP_CAPTURE__SCOPED_DESKTOP_EFFECTS_H

#include "aspia_config.h"

#include <memory>

#include "base/scoped_native_library.h"
#include "base/macros.h"

namespace aspia {

class ScopedDesktopEffects
{
public:
    ScopedDesktopEffects();
    ~ScopedDesktopEffects();

private:
    bool IsCompositionEnabled();
    void EnableComposition(bool enable);

private:
    std::unique_ptr<ScopedNativeLibrary> dwapi_;

    bool composition_enabled_;

    std::wstring wallpaper_;

    bool drag_full_windows_;
    bool drop_shadow_;
    bool animation_;
    bool menu_animation_;
    bool selection_fade_;
    bool tooltip_animation_;
    bool cursor_shadow_;
    bool combobox_animation_;
    bool ui_effects_;
    bool client_area_animation_;
    bool gradient_captions_;
    bool listbox_smooth_scrolling_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDesktopEffects);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__SCOPED_DESKTOP_EFFECTS_H
