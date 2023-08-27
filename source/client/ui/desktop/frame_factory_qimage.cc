//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/desktop/frame_factory_qimage.h"

#include "client/ui/desktop/frame_qimage.h"

namespace client {

//--------------------------------------------------------------------------------------------------
FrameFactoryQImage::FrameFactoryQImage() = default;

//--------------------------------------------------------------------------------------------------
FrameFactoryQImage::~FrameFactoryQImage() = default;

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::Frame> FrameFactoryQImage::allocateFrame(const base::Size& size)
{
    return std::shared_ptr<base::Frame>(FrameQImage::create(size).release());
}

} // namespace client
