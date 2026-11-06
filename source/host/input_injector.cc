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

#include "host/input_injector.h"

#include <limits>

//--------------------------------------------------------------------------------------------------
// static
QPoint InputInjector::mapToScreen(const QPoint& position, const QSize& frame_size, const QSize& screen_size)
{
    // The last pixel of the frame maps onto the last pixel of the screen. The client coordinates are
    // untrusted, so the result is clamped before the narrowing cast.
    auto map = [](int value, int frame, int screen)
    {
        if (frame <= 1)
            return 0;

        const double result = static_cast<double>(value) * (screen - 1) / (frame - 1);

        const double min = std::numeric_limits<int>::min();
        const double max = std::numeric_limits<int>::max();

        return static_cast<int>(std::clamp(result, min, max));
    };

    return QPoint(map(position.x(), frame_size.width(), screen_size.width()),
                  map(position.y(), frame_size.height(), screen_size.height()));
}
