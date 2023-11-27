//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_DESKTOP_DESKTOP_RESIZER_X11_H
#define BASE_DESKTOP_DESKTOP_RESIZER_X11_H

#include "base/macros_magic.h"
#include "base/desktop/desktop_resizer.h"

namespace base {

class DesktopResizerX11 : public DesktopResizer
{
public:
    DesktopResizerX11();
    ~DesktopResizerX11() override;

    // DesktopResizer implementation.
    std::vector<Size> supportedResolutions(ScreenId screen_id) override;
    bool setResolution(ScreenId screen_id, const Size& resolution) override;
    void restoreResolution(ScreenId screen_id) override;
    void restoreResulution() override;

private:
    DISALLOW_COPY_AND_ASSIGN(DesktopResizerX11);
};

} // namespace base

#endif // BASE_DESKTOP_DESKTOP_RESIZER_X11_H
