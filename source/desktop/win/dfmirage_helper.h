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

#ifndef DESKTOP__WIN__DFMIRAGE_HELPER_H
#define DESKTOP__WIN__DFMIRAGE_HELPER_H

#include "base/win/scoped_hdc.h"
#include "desktop/win/dfmirage.h"
#include "desktop/win/mirror_helper.h"

namespace desktop {

class DFMirageHelper : public MirrorHelper
{
public:
    ~DFMirageHelper();

    static std::unique_ptr<DFMirageHelper> create(const Rect& screen_rect);

    const Rect& screenRect() const override { return screen_rect_; }
    void addUpdatedRects(Region* updated_region) const override;
    void copyRegion(Frame* frame, const Region& updated_region) const override;

private:
    explicit DFMirageHelper(const Rect& screen_rect);

    bool update(bool load);
    bool mapMemory(bool map);

    bool is_attached_ = false;
    bool is_updated_ = false;
    bool is_mapped_ = false;

    std::wstring device_key_;
    std::wstring device_name_;

    base::win::ScopedCreateDC driver_dc_;
    const Rect screen_rect_;

    DfmGetChangesBuffer get_changes_buffer_;

    mutable int last_update_ = 0;

    DISALLOW_COPY_AND_ASSIGN(DFMirageHelper);
};

} // namespace desktop

#endif // DESKTOP__WIN__DFMIRAGE_HELPER_H
