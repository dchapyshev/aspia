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

#include "desktop/win/dxgi_adapter_duplicator.h"

#include "base/logging.h"

#include <algorithm>

#include <DXGI.h>
#include <comdef.h>

namespace desktop {

using Microsoft::WRL::ComPtr;

namespace {

bool isValidRect(const RECT& rect)
{
    return rect.right > rect.left && rect.bottom > rect.top;
}

}  // namespace

DxgiAdapterDuplicator::DxgiAdapterDuplicator(const D3dDevice& device)
    : device_(device)
{
    // Nothing
}

DxgiAdapterDuplicator::DxgiAdapterDuplicator(DxgiAdapterDuplicator&&) = default;
DxgiAdapterDuplicator::~DxgiAdapterDuplicator() = default;

bool DxgiAdapterDuplicator::initialize()
{
    if (doInitialize())
        return true;

    duplicators_.clear();
    return false;
}

bool DxgiAdapterDuplicator::doInitialize()
{
    for (int i = 0;; ++i)
    {
        ComPtr<IDXGIOutput> output;

        _com_error error = device_.dxgiAdapter()->EnumOutputs(i, output.GetAddressOf());
        if (error.Error() == DXGI_ERROR_NOT_FOUND)
            break;

        if (error.Error() == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            LOG(LS_WARNING) << "IDXGIAdapter::EnumOutputs returns NOT_CURRENTLY_AVAILABLE. "
                               "This may happen when running in session 0";
            break;
        }

        if (error.Error() != S_OK || !output)
        {
            LOG(LS_WARNING) << "IDXGIAdapter::EnumOutputs returns an unexpected result "
                            << error.ErrorMessage() << " with error code"
                            << error.Error();
            continue;
        }

        DXGI_OUTPUT_DESC desc;
        error = output->GetDesc(&desc);
        if (error.Error() == S_OK)
        {
            if (desc.AttachedToDesktop && isValidRect(desc.DesktopCoordinates))
            {
                ComPtr<IDXGIOutput1> output1;
                error = output.As(&output1);

                if (error.Error() != S_OK || !output1)
                {
                    LOG(LS_WARNING) << "Failed to convert IDXGIOutput to IDXGIOutput1, this "
                                       "usually means the system does not support DirectX 11";
                    continue;
                }

                DxgiOutputDuplicator duplicator(device_, output1, desc);
                if (!duplicator.initialize())
                {
                    LOG(LS_WARNING) << "Failed to initialize DxgiOutputDuplicator on output " << i;
                    continue;
                }

                duplicators_.push_back(std::move(duplicator));

                desktop_rect_.unionWith(duplicators_.back().desktopRect());
            }
            else
            {
                LOG(LS_ERROR) << (desc.AttachedToDesktop ? "Attached" : "Detached")
                              << " output " << i << " ("
                              << desc.DesktopCoordinates.top << ", "
                              << desc.DesktopCoordinates.left << ") - ("
                              << desc.DesktopCoordinates.bottom << ", "
                              << desc.DesktopCoordinates.right << ") is ignored";
            }
        }
        else
        {
            LOG(LS_WARNING) << "Failed to get output description of device " << i << ", ignore";
        }
    }

    if (duplicators_.empty())
    {
        LOG(LS_WARNING) << "Cannot initialize any DxgiOutputDuplicator instance";
    }

    return !duplicators_.empty();
}

void DxgiAdapterDuplicator::setup(Context* context)
{
    DCHECK(context->contexts.empty());

    context->contexts.resize(duplicators_.size());

    for (size_t i = 0; i < duplicators_.size(); ++i)
        duplicators_[i].setup(&context->contexts[i]);
}

void DxgiAdapterDuplicator::unregister(const Context* const context)
{
    DCHECK_EQ(context->contexts.size(), duplicators_.size());

    for (size_t i = 0; i < duplicators_.size(); ++i)
        duplicators_[i].unregister(&context->contexts[i]);
}

bool DxgiAdapterDuplicator::duplicate(Context* context, SharedFrame* target)
{
    DCHECK_EQ(context->contexts.size(), duplicators_.size());

    for (size_t i = 0; i < duplicators_.size(); ++i)
    {
        if (!duplicators_[i].duplicate(&context->contexts[i],
                                       duplicators_[i].desktopRect().topLeft(),
                                       target))
        {
            return false;
        }
    }

    return true;
}

bool DxgiAdapterDuplicator::duplicateMonitor(Context* context, int monitor_id, SharedFrame* target)
{
    DCHECK_GE(monitor_id, 0);
    DCHECK_LT(monitor_id, static_cast<int>(duplicators_.size()));
    DCHECK_EQ(context->contexts.size(), duplicators_.size());

    return duplicators_[monitor_id].duplicate(&context->contexts[monitor_id], Point(), target);
}

Rect DxgiAdapterDuplicator::screenRect(int id) const
{
    DCHECK_GE(id, 0);
    DCHECK_LT(id, static_cast<int>(duplicators_.size()));

    return duplicators_[id].desktopRect();
}

const std::wstring& DxgiAdapterDuplicator::deviceName(int id) const
{
    DCHECK_GE(id, 0);
    DCHECK_LT(id, static_cast<int>(duplicators_.size()));

    return duplicators_[id].deviceName();
}

int DxgiAdapterDuplicator::screenCount() const
{
    return static_cast<int>(duplicators_.size());
}

int64_t DxgiAdapterDuplicator::numFramesCaptured() const
{
    int64_t min = std::numeric_limits<int64_t>::max();

    for (const auto& duplicator : duplicators_)
        min = std::min(min, duplicator.numFramesCaptured());

    return min;
}

void DxgiAdapterDuplicator::translateRect(const Point& position)
{
    desktop_rect_.translate(position);

    DCHECK_GE(desktop_rect_.left(), 0);
    DCHECK_GE(desktop_rect_.top(), 0);

    for (auto& duplicator : duplicators_)
        duplicator.translateRect(position);
}

} // namespace desktop
