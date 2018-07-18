//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_H

#include <memory>

#include "desktop_capture/pixel_format.h"

namespace aspia {

class PixelTranslator
{
public:
    virtual ~PixelTranslator() = default;

    static std::unique_ptr<PixelTranslator> create(const PixelFormat& source_format,
                                                   const PixelFormat& target_format);

    virtual void translate(const quint8* src,
                           int src_stride,
                           quint8* dst,
                           int dst_stride,
                           int width,
                           int height) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_H
