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

#include "base/win/scoped_com_initializer.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedCOMInitializer::ScopedCOMInitializer()
{
    initialize(COINIT_APARTMENTTHREADED);
}

//--------------------------------------------------------------------------------------------------
ScopedCOMInitializer::ScopedCOMInitializer(SelectMTA /* mta */)
{
    initialize(COINIT_MULTITHREADED);
}

//--------------------------------------------------------------------------------------------------
ScopedCOMInitializer::~ScopedCOMInitializer()
{
    if (isSucceeded())
        CoUninitialize();
}

//--------------------------------------------------------------------------------------------------
bool ScopedCOMInitializer::isSucceeded() const
{
    return SUCCEEDED(hr_);
}

//--------------------------------------------------------------------------------------------------
void ScopedCOMInitializer::initialize(COINIT init)
{
    hr_ = CoInitializeEx(nullptr, init);
    DCHECK_NE(hr_, RPC_E_CHANGED_MODE) << "Invalid COM thread model change";
}

} // namespace base
