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

#ifndef DESKTOP__WIN__MV2_HELPER_H
#define DESKTOP__WIN__MV2_HELPER_H

#include "base/win/scoped_hdc.h"
#include "base/win/scoped_object.h"
#include "desktop/win/mv2.h"
#include "desktop/win/mirror_helper.h"

namespace desktop {

class Mv2Helper : public MirrorHelper
{
public:
    ~Mv2Helper();

    static std::unique_ptr<Mv2Helper> create(const Rect& screen_rect);

    const Rect& screenRect() const override { return screen_rect_; }
    void addUpdatedRects(Region* updated_region) const override;
    void copyRegion(Frame* frame, const Region& updated_region) const override;

private:
    explicit Mv2Helper(const Rect& screen_rect);

    bool update(bool load);
    bool mapMemory(bool map);

    bool is_attached_ = false;
    bool is_updated_ = false;
    bool is_mapped_ = false;

    std::wstring device_key_;
    std::wstring device_name_;

    base::win::ScopedCreateDC driver_dc_;
    const Rect screen_rect_;

    uint8_t* shared_buffer_ = nullptr;

    uint8_t* screen_buffer_ = nullptr;
    Mv2ChangesBuffer* changes_buffer_ = nullptr;

    mutable int last_update_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Mv2Helper);
};

} // namespace desktop

#endif // DESKTOP__WIN__MV2_HELPER_H
