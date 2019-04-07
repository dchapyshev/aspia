//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__DFMIRAGE_HELPER_H
#define DESKTOP__DFMIRAGE_HELPER_H

#include "base/win/scoped_hdc.h"
#include "desktop/dfmirage.h"
#include "desktop/desktop_geometry.h"

namespace desktop {

class DFMirageHelper
{
public:
    ~DFMirageHelper();

    static std::unique_ptr<DFMirageHelper> create(const Rect& desktop_rect);

    const Rect& screenRect() const { return screen_rect_; }

    const uint8_t* screenBuffer() { return get_changes_buffer_.user_buffer; }
    DfmChangesBuffer* changesBuffer() { return get_changes_buffer_.changes_buffer; }

private:
    explicit DFMirageHelper(const Rect& screen_rect);

    bool update(bool load);
    bool mapMemory(bool map);

    bool findDisplayDevice();
    bool attachToDesktop(bool attach);

    std::wstring device_key_;
    std::wstring device_name_;

    base::win::ScopedCreateDC driver_dc_;
    const Rect screen_rect_;

    DfmGetChangesBuffer get_changes_buffer_;

    DISALLOW_COPY_AND_ASSIGN(DFMirageHelper);
};

} // namespace desktop

#endif // DESKTOP__DFMIRAGE_HELPER_H
