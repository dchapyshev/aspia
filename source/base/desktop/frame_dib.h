//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_FRAME_DIB_H
#define BASE_DESKTOP_FRAME_DIB_H

#include "base/desktop/frame.h"
#include "base/ipc/shared_memory.h"
#include "base/win/scoped_gdi_object.h"

#include <memory>

namespace base {

class SharedMemoryFactory;

class FrameDib final : public Frame
{
public:
    ~FrameDib() final = default;

    static std::unique_ptr<FrameDib> create(const Size& size,
                                            const PixelFormat& format,
                                            SharedMemoryFactory* shared_memory_factory,
                                            HDC hdc);

    HBITMAP bitmap() { return bitmap_; }

private:
    FrameDib(const Size& size,
             const PixelFormat& format,
             int stride,
             quint8* data,
             std::unique_ptr<SharedMemory> shared_memory,
             HBITMAP bitmap);

    ScopedHBITMAP bitmap_;
    std::unique_ptr<SharedMemory> owned_shared_memory_;

    Q_DISABLE_COPY(FrameDib)
};

} // namespace base

#endif // BASE_DESKTOP_FRAME_DIB_H
