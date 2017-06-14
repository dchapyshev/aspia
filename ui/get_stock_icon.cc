//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/get_stock_icon.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/get_stock_icon.h"
#include "base/version_helpers.h"

#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>

namespace aspia {

static const WCHAR kShellIcons[] =
L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Icons";

static const WCHAR kShell32LibraryName[] = L"shell32.dll";

HRESULT GetStockIconInfo(SHSTOCKICONID id, UINT flags, SHSTOCKICONINFO* sii)
{
#if (NTDDI_VERSION >= NTDDI_VISTA)
    return SHGetStockIconInfo(id, flags, sii);
#else
    if (IsWindowsVistaOrGreater())
    {
        typedef HRESULT(WINAPI *SHGetStockIconInfoFn)(SHSTOCKICONID, UINT, SHSTOCKICONINFO*);

        HMODULE shell32_module = GetModuleHandleW(kShell32LibraryName);

        if (!shell32_module)
            return E_UNEXPECTED;

        SHGetStockIconInfoFn get_stock_icon =
            reinterpret_cast<SHGetStockIconInfoFn>(
                GetProcAddress(shell32_module, "SHGetStockIconInfo"));

        if (!get_stock_icon)
            return E_UNEXPECTED;

        return get_stock_icon(id, flags, sii);
    }

    if (id < 0 || id >= SIID_MAX_ICONS)
        return E_INVALIDARG;

    if (!sii || sii->cbSize != sizeof(SHSTOCKICONINFO))
        return E_INVALIDARG;

    sii->iSysImageIndex = -1;
    sii->iIcon          = -1;
    sii->hIcon          = nullptr;
    sii->szPath[0]      = 0;

    WCHAR value_name[12];

    if (SUCCEEDED(StringCchPrintfW(value_name, ARRAYSIZE(value_name), L"%d", id)))
    {
        DWORD path_size = sizeof(sii->szPath);

        if (SHRegGetValueW(HKEY_LOCAL_MACHINE,
                           kShellIcons,
                           value_name,
                           SRRF_RT_REG_SZ,
                           REG_NONE,
                           sii->szPath,
                           &path_size) == ERROR_SUCCESS)
        {
            sii->iIcon = PathParseIconLocationW(sii->szPath);
        }
        else
        {
            sii->iIcon = -id;

            UINT ret = GetSystemDirectoryW(sii->szPath, MAX_PATH);
            if (!ret || ret >= MAX_PATH)
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);

            if (!PathAppendW(sii->szPath, kShell32LibraryName))
                return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        }
    }

    if (sii->iIcon == -1)
        sii->iIcon = SIID_DOCNOASSOC;

    if (flags & SHGSI_ICON)
    {
        if (flags & SHGSI_SMALLICON)
        {
            ExtractIconExW(sii->szPath, id, nullptr, &sii->hIcon, 1);
        }
        else
        {
            ExtractIconExW(sii->szPath, id, &sii->hIcon, nullptr, 1);
        }
    }

    if (flags & SHGSI_SYSICONINDEX)
    {
        sii->iSysImageIndex = Shell_GetCachedImageIndex(sii->szPath, sii->iIcon, 0);
    }

    return S_OK;
#endif // (NTDDI_VERSION >= NTDDI_VISTA)
}

} // namespace aspia
