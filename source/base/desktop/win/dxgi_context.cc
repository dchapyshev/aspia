//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/win/dxgi_context.h"

#include "base/desktop/win/dxgi_duplicator_controller.h"

namespace base {

//--------------------------------------------------------------------------------------------------
DxgiAdapterContext::DxgiAdapterContext() = default;

//--------------------------------------------------------------------------------------------------
DxgiAdapterContext::DxgiAdapterContext(const DxgiAdapterContext& context) = default;

//--------------------------------------------------------------------------------------------------
DxgiAdapterContext& DxgiAdapterContext::operator=(const DxgiAdapterContext& other) = default;

//--------------------------------------------------------------------------------------------------
DxgiAdapterContext::~DxgiAdapterContext() = default;

//--------------------------------------------------------------------------------------------------
DxgiFrameContext::DxgiFrameContext(std::shared_ptr<DxgiDuplicatorController> controller)
    : controller(std::move(controller))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DxgiFrameContext::~DxgiFrameContext()
{
    reset();
}

//--------------------------------------------------------------------------------------------------
void DxgiFrameContext::reset()
{
    controller->unregister(this);
    controller_id = 0;
}

} // namespace base
