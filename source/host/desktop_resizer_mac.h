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

#ifndef HOST_DESKTOP_RESIZER_MAC_H
#define HOST_DESKTOP_RESIZER_MAC_H

#include <memory>

#include "host/desktop_resizer.h"

class DesktopResizerMac final : public DesktopResizer
{
public:
    DesktopResizerMac();
    ~DesktopResizerMac() final;

    // DesktopResizer implementation.
    QList<QSize> supportedResolutions(ScreenId screen_id) final;
    bool setResolution(ScreenId screen_id, const QSize& resolution) final;
    void restoreResolution(ScreenId screen_id) final;
    void restoreResulution() final;

private:
    struct Context;
    std::unique_ptr<Context> context_;

    Q_DISABLE_COPY_MOVE(DesktopResizerMac)
};

#endif // HOST_DESKTOP_RESIZER_MAC_H
