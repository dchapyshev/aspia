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

#ifndef BASE_CODEC_MF_RUNTIME_H
#define BASE_CODEC_MF_RUNTIME_H

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>

// mfapi.h gates this GUID behind WINVER >= _WIN32_WINNT_WIN8 but the project is built with
// _WIN32_WINNT=0x0601. EXTERN_GUID with DECLSPEC_SELECTANY is self-contained so we do not pull a
// load-time dependency on mfuuid.lib.
EXTERN_GUID(MF_LOW_LATENCY, 0x9c27891a, 0xed7a, 0x40e1, 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee);

namespace mf {

// Loads mfplat.dll, mf.dll and d3d11.dll on first use and resolves the entry
// points used by the H264 encoder/decoder. Returns false on stripped Windows
// installations where any of these libraries or symbols are missing (e.g.
// Server Core without the Media Foundation feature pack). All wrappers below
// return E_NOTIMPL when the runtime is unavailable.
bool isRuntimeAvailable();

HRESULT startup(ULONG version, DWORD flags);
HRESULT shutdown();
HRESULT createMediaType(IMFMediaType** out);
HRESULT createSample(IMFSample** out);
HRESULT createAlignedMemoryBuffer(DWORD max_length, DWORD alignment, IMFMediaBuffer** out);
HRESULT createDxgiSurfaceBuffer(REFIID riid, IUnknown* surface, UINT sub_index,
                                BOOL bottom_up, IMFMediaBuffer** out);
HRESULT createDxgiDeviceManager(UINT* reset_token, IMFDXGIDeviceManager** out);
HRESULT enumTransforms(GUID category, UINT32 flags,
                       const MFT_REGISTER_TYPE_INFO* input_type,
                       const MFT_REGISTER_TYPE_INFO* output_type,
                       IMFActivate*** activate_arr, UINT32* count);
HRESULT d3d11CreateDevice(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type,
                          HMODULE software, UINT flags,
                          const D3D_FEATURE_LEVEL* feature_levels, UINT num_levels,
                          UINT sdk_version, ID3D11Device** device,
                          D3D_FEATURE_LEVEL* feature_level,
                          ID3D11DeviceContext** context);

} // namespace mf

#endif // BASE_CODEC_MF_RUNTIME_H
