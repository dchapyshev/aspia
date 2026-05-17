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

#include "base/codec/mf_runtime.h"

#include "base/logging.h"

namespace mf {

namespace {

using PFN_MFStartup = decltype(&::MFStartup);
using PFN_MFShutdown = decltype(&::MFShutdown);
using PFN_MFCreateMediaType = decltype(&::MFCreateMediaType);
using PFN_MFCreateSample = decltype(&::MFCreateSample);
using PFN_MFCreateAlignedMemoryBuffer = decltype(&::MFCreateAlignedMemoryBuffer);
using PFN_MFCreateDXGISurfaceBuffer = decltype(&::MFCreateDXGISurfaceBuffer);
using PFN_MFCreateDXGIDeviceManager = decltype(&::MFCreateDXGIDeviceManager);
using PFN_MFTEnumEx = decltype(&::MFTEnumEx);
using PFN_D3D11CreateDevice = decltype(&::D3D11CreateDevice);

HMODULE h_mfplat = nullptr;
HMODULE h_mf = nullptr;
HMODULE h_d3d11 = nullptr;

PFN_MFStartup pfn_MFStartup = nullptr;
PFN_MFShutdown pfn_MFShutdown = nullptr;
PFN_MFCreateMediaType pfn_MFCreateMediaType = nullptr;
PFN_MFCreateSample pfn_MFCreateSample = nullptr;
PFN_MFCreateAlignedMemoryBuffer pfn_MFCreateAlignedMemoryBuffer = nullptr;
PFN_MFCreateDXGISurfaceBuffer pfn_MFCreateDXGISurfaceBuffer = nullptr;
PFN_MFCreateDXGIDeviceManager pfn_MFCreateDXGIDeviceManager = nullptr;
PFN_MFTEnumEx pfn_MFTEnumEx = nullptr;
PFN_D3D11CreateDevice pfn_D3D11CreateDevice = nullptr;

//--------------------------------------------------------------------------------------------------
template <typename Fn>
bool resolve(HMODULE module, const char* name, Fn* slot)
{
    *slot = reinterpret_cast<Fn>(GetProcAddress(module, name));
    if (!*slot)
    {
        LOG(WARNING) << "MF runtime missing entry point:" << name;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool tryLoad()
{
    static const bool result = []()
    {
        h_mfplat = LoadLibraryW(L"mfplat.dll");
        h_mf = LoadLibraryW(L"mf.dll");
        h_d3d11 = LoadLibraryW(L"d3d11.dll");

        if (!h_mfplat || !h_mf || !h_d3d11)
        {
            LOG(WARNING) << "MF runtime DLLs not available on this system";
            return false;
        }

        bool ok = true;
        ok &= resolve(h_mfplat, "MFStartup", &pfn_MFStartup);
        ok &= resolve(h_mfplat, "MFShutdown", &pfn_MFShutdown);
        ok &= resolve(h_mfplat, "MFCreateMediaType", &pfn_MFCreateMediaType);
        ok &= resolve(h_mfplat, "MFCreateSample", &pfn_MFCreateSample);
        ok &= resolve(h_mfplat, "MFCreateAlignedMemoryBuffer", &pfn_MFCreateAlignedMemoryBuffer);
        ok &= resolve(h_mfplat, "MFCreateDXGISurfaceBuffer", &pfn_MFCreateDXGISurfaceBuffer);
        ok &= resolve(h_mfplat, "MFCreateDXGIDeviceManager", &pfn_MFCreateDXGIDeviceManager);
        ok &= resolve(h_mf, "MFTEnumEx", &pfn_MFTEnumEx);
        ok &= resolve(h_d3d11, "D3D11CreateDevice", &pfn_D3D11CreateDevice);
        return ok;
    }();
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool isRuntimeAvailable()
{
    return tryLoad();
}

//--------------------------------------------------------------------------------------------------
HRESULT startup(ULONG version, DWORD flags)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFStartup(version, flags);
}

//--------------------------------------------------------------------------------------------------
HRESULT shutdown()
{
    if (!pfn_MFShutdown)
        return E_NOTIMPL;
    return pfn_MFShutdown();
}

//--------------------------------------------------------------------------------------------------
HRESULT createMediaType(IMFMediaType** out)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFCreateMediaType(out);
}

//--------------------------------------------------------------------------------------------------
HRESULT createSample(IMFSample** out)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFCreateSample(out);
}

//--------------------------------------------------------------------------------------------------
HRESULT createAlignedMemoryBuffer(DWORD max_length, DWORD alignment, IMFMediaBuffer** out)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFCreateAlignedMemoryBuffer(max_length, alignment, out);
}

//--------------------------------------------------------------------------------------------------
HRESULT createDxgiSurfaceBuffer(REFIID riid, IUnknown* surface, UINT sub_index,
                                BOOL bottom_up, IMFMediaBuffer** out)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFCreateDXGISurfaceBuffer(riid, surface, sub_index, bottom_up, out);
}

//--------------------------------------------------------------------------------------------------
HRESULT createDxgiDeviceManager(UINT* reset_token, IMFDXGIDeviceManager** out)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFCreateDXGIDeviceManager(reset_token, out);
}

//--------------------------------------------------------------------------------------------------
HRESULT enumTransforms(GUID category, UINT32 flags,
                       const MFT_REGISTER_TYPE_INFO* input_type,
                       const MFT_REGISTER_TYPE_INFO* output_type,
                       IMFActivate*** activate_arr, UINT32* count)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_MFTEnumEx(category, flags, input_type, output_type, activate_arr, count);
}

//--------------------------------------------------------------------------------------------------
HRESULT d3d11CreateDevice(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type,
                          HMODULE software, UINT flags,
                          const D3D_FEATURE_LEVEL* feature_levels, UINT num_levels,
                          UINT sdk_version, ID3D11Device** device,
                          D3D_FEATURE_LEVEL* feature_level,
                          ID3D11DeviceContext** context)
{
    if (!tryLoad())
        return E_NOTIMPL;
    return pfn_D3D11CreateDevice(adapter, driver_type, software, flags, feature_levels, num_levels,
        sdk_version, device, feature_level, context);
}

} // namespace mf
