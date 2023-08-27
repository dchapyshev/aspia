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

#include "base/ipc/shared_memory_factory_proxy.h"

#include "base/logging.h"
#include "base/ipc/shared_memory_factory.h"

namespace base {

//--------------------------------------------------------------------------------------------------
SharedMemoryFactoryProxy::SharedMemoryFactoryProxy(SharedMemoryFactory* factory)
    : factory_(factory)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SharedMemoryFactoryProxy::~SharedMemoryFactoryProxy()
{
    DCHECK(!factory_);
}

//--------------------------------------------------------------------------------------------------
void SharedMemoryFactoryProxy::dettach()
{
    factory_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void SharedMemoryFactoryProxy::onSharedMemoryCreate(int id)
{
    if (!factory_)
        return;

    factory_->onSharedMemoryCreate(id);
}

//--------------------------------------------------------------------------------------------------
void SharedMemoryFactoryProxy::onSharedMemoryDestroy(int id)
{
    if (!factory_)
        return;

    factory_->onSharedMemoryDestroy(id);
}

} // namespace base
