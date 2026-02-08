//
// SmartCafe Project
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

#ifndef BASE_CODEC_PIXEL_TRANSLATOR_H
#define BASE_CODEC_PIXEL_TRANSLATOR_H

#include "base/desktop/pixel_format.h"

#include <memory>

namespace base {

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

} // namespace base

#endif // BASE_CODEC_PIXEL_TRANSLATOR_H
