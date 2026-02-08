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

#include "base/desktop/screen_capturer.h"

#include "base/ipc/shared_memory_factory.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenCapturer(Type type, QObject* parent)
    : QObject(parent),
      type_(type)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturer::setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory)
{
    shared_memory_factory_ = shared_memory_factory;
}

//--------------------------------------------------------------------------------------------------
SharedMemoryFactory* ScreenCapturer::sharedMemoryFactory() const
{
    return shared_memory_factory_;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::Type ScreenCapturer::type() const
{
    return type_;
}

} // namespace base
