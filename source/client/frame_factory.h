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

#ifndef CLIENT__FRAME_FACTORY_H
#define CLIENT__FRAME_FACTORY_H

#include <memory>

namespace desktop {
class Frame;
class Size;
} // namespace desktop

namespace client {

class FrameFactory
{
public:
    virtual ~FrameFactory() = default;

    virtual std::shared_ptr<desktop::Frame> allocateFrame(const desktop::Size& size) = 0;
};

} // namespace client

#endif // CLIENT__FRAME_FACTORY_H
