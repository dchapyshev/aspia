//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_FRAME_FACTORY_QIMAGE_H
#define CLIENT_UI_DESKTOP_FRAME_FACTORY_QIMAGE_H

#include "base/macros_magic.h"
#include "client/frame_factory.h"

namespace client {

class FrameFactoryQImage final : public FrameFactory
{
public:
    FrameFactoryQImage();
    ~FrameFactoryQImage() final;

    std::shared_ptr<base::Frame> allocateFrame(const base::Size& size) final;

private:
    DISALLOW_COPY_AND_ASSIGN(FrameFactoryQImage);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_FRAME_FACTORY_QIMAGE_H
