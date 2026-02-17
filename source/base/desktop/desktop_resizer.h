//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_DESKTOP_RESIZER_H
#define BASE_DESKTOP_DESKTOP_RESIZER_H

#include "base/desktop/screen_capturer.h"

#include <memory>

namespace base {

class DesktopResizer
{
public:
    virtual ~DesktopResizer() = default;

    using ScreenId = ScreenCapturer::ScreenId;

    // Create a platform-specific DesktopResizer instance.
    static std::unique_ptr<DesktopResizer> create();

    virtual QList<QSize> supportedResolutions(ScreenId screen_id) = 0;
    virtual bool setResolution(ScreenId screen_id, const QSize& resolution) = 0;
    virtual void restoreResolution(ScreenId screen_id) = 0;
    virtual void restoreResulution() = 0;
};

} // namespace base

#endif // BASE_DESKTOP_DESKTOP_RESIZER_H
