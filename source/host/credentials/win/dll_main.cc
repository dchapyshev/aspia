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

#include "host/credentials/win/dll_main.h"

#include <initguid.h>
#include <shlwapi.h>
#include <unknwn.h>
#include <wrl/client.h>

#include <atomic>
#include <new>

#include "host/credentials/win/logging.h"
#include "host/credentials/win/provider.h"

using Microsoft::WRL::ComPtr;

static std::atomic<ULONG> g_ref_count = 0;
HINSTANCE g_instance = nullptr;

// 117ab3c4-881d-4914-a680-2a9d16e28580
DEFINE_GUID(CLSID_AspiaCredentials, 0x117ab3c4, 0x881d, 0x4914, 0xa6, 0x80, 0x2a, 0x9d, 0x16, 0xe2, 0x85, 0x80);

class ClassFactory final : public IClassFactory
{
public:
    ClassFactory() = default;
    ~ClassFactory() = default;

    // IUnknown implementation.
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) final;
    IFACEMETHODIMP_(ULONG) AddRef() final;
    IFACEMETHODIMP_(ULONG) Release() final;

    // IClassFactory implementation.
    IFACEMETHODIMP CreateInstance(IUnknown* unknown_outer, REFIID riid, void** ppv) final;
    IFACEMETHODIMP LockServer(BOOL lock) final;

private:
    std::atomic<ULONG> ref_count_ = 1;
};

//--------------------------------------------------------------------------------------------------
HRESULT ClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        LOG(L"Invalid argument");
        return E_POINTER;
    }

    static const QITAB qit[] = { QITABENT(ClassFactory, IClassFactory), { 0 }, };
    return QISearch(this, qit, riid, ppv);
}

//--------------------------------------------------------------------------------------------------
ULONG ClassFactory::AddRef()
{
    return ++ref_count_;
}

//--------------------------------------------------------------------------------------------------
ULONG ClassFactory::Release()
{
    ULONG ref_count = --ref_count_;
    if (!ref_count)
        delete this;
    return ref_count;
}

//--------------------------------------------------------------------------------------------------
HRESULT ClassFactory::CreateInstance(IUnknown* unknown_outer, REFIID riid, void** ppv)
{
    if (!ppv)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *ppv = nullptr;

    if (unknown_outer)
    {
        LOG(L"Aggregation is not supported");
        return CLASS_E_NOAGGREGATION;
    }

    return Provider::create(riid, ppv);
}

//--------------------------------------------------------------------------------------------------
HRESULT ClassFactory::LockServer(BOOL lock)
{
    if (lock)
        moduleAddRef();
    else
        moduleRelease();
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
void moduleAddRef()
{
    ++g_ref_count;
}

//--------------------------------------------------------------------------------------------------
void moduleRelease()
{
    --g_ref_count;
}

//--------------------------------------------------------------------------------------------------
STDAPI DllCanUnloadNow()
{
    return (g_ref_count > 0) ? S_FALSE : S_OK;
}

//--------------------------------------------------------------------------------------------------
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv)
    {
        LOG(L"Invalid argument");
        return E_INVALIDARG;
    }

    *ppv = nullptr;

    if (GetEnvironmentVariableW(L"ASPIA_DISABLE_CREDENTIALS", nullptr, 0) != 0)
    {
        LOG(L"Disabled via ASPIA_DISABLE_CREDENTIALS");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    if (CLSID_AspiaCredentials != rclsid)
    {
        LOG(L"Requested class is not available");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ComPtr<ClassFactory> factory;
    factory.Attach(new(std::nothrow) ClassFactory());
    if (!factory)
    {
        LOG(L"ClassFactory allocation failed");
        return E_OUTOFMEMORY;
    }

    return factory.CopyTo(riid, ppv);
}

//--------------------------------------------------------------------------------------------------
STDAPI_(BOOL) DllMain(HINSTANCE hinstance, DWORD reason, void*)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstance);
            break;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    g_instance = hinstance;
    return TRUE;
}
