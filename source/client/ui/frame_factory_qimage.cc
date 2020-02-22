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

#include "client/ui/frame_factory_qimage.h"

#include "client/ui/frame_qimage.h"

namespace client {

FrameFactoryQImage::FrameFactoryQImage() = default;

FrameFactoryQImage::~FrameFactoryQImage() = default;

std::shared_ptr<desktop::Frame> FrameFactoryQImage::allocateFrame(const desktop::Size& size)
{
    return std::shared_ptr<desktop::Frame>(FrameQImage::create(size).release());
}

} // namespace client
