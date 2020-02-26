//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__WIN__MIRROR_HELPER_H
#define DESKTOP__WIN__MIRROR_HELPER_H

#include "desktop/desktop_geometry.h"

#include <string>

namespace desktop {

class Frame;
class Region;

class MirrorHelper
{
public:
    virtual ~MirrorHelper() = default;

    virtual const Rect& screenRect() const = 0;
    virtual void addUpdatedRects(Region* updated_region) const = 0;
    virtual void copyRegion(Frame* frame, const Region& updated_region) const = 0;

protected:
    static bool findDisplayDevice(std::wstring_view device_string,
                                  std::wstring* device_name,
                                  std::wstring* device_key);
    static bool attachToDesktop(std::wstring_view key_path, bool attach);
};

} // namespace desktop

#endif // DESKTOP__WIN__MIRROR_HELPER_H
