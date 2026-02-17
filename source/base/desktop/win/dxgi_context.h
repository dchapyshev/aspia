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

#ifndef BASE_DESKTOP_WIN_DXGI_CONTEXT_H
#define BASE_DESKTOP_WIN_DXGI_CONTEXT_H

#include <QRegion>

#include <memory>
#include <vector>

namespace base {

class DxgiDuplicatorController;

// A DxgiOutputContext stores the status of a single DxgiFrame of DxgiOutputDuplicator.
struct DxgiOutputContext final
{
    // The updated region DxgiOutputDuplicator::detectUpdatedRegion() output during last
    // duplicate() function call. It's always relative to the (0, 0).
    QRegion updated_region;
};

// A DxgiAdapterContext stores the status of a single DxgiFrame of DxgiAdapterDuplicator.
struct DxgiAdapterContext final
{
    DxgiAdapterContext();
    DxgiAdapterContext(const DxgiAdapterContext& other);
    DxgiAdapterContext& operator=(const DxgiAdapterContext& other);
    ~DxgiAdapterContext();

    // Child DxgiOutputContext belongs to this AdapterContext.
    std::vector<DxgiOutputContext> contexts;
};

// A DxgiFrameContext stores the status of a single DxgiFrame of DxgiDuplicatorController.
struct DxgiFrameContext final
{
public:
    explicit DxgiFrameContext(std::shared_ptr<DxgiDuplicatorController> controller);

    // Unregister this Context instance from DxgiDuplicatorController during destructing.
    ~DxgiFrameContext();

    // Reset current Context, so it will be reinitialized next time.
    void reset();

    // A Context will have an exactly same |controller_id| as DxgiDuplicatorController, to ensure
    // it has been correctly setted up after each DxgiDuplicatorController::initialize().
    int controller_id = 0;

    // Child DxgiAdapterContext belongs to this DxgiFrameContext.
    std::vector<DxgiAdapterContext> contexts;
    std::shared_ptr<DxgiDuplicatorController> controller;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_CONTEXT_H
