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

#include "base/win/resource_util.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
bool resourceFromModule(HMODULE module, int resource_id, const wchar_t* resource_type,
                        void** data, size_t* length)
{
    if (!module)
    {
        LOG(ERROR) << "Invalid module handle";
        return false;
    }

    if (!IS_INTRESOURCE(resource_id))
    {
        NOTREACHED();
        return false;
    }

    HRSRC hres_info = FindResourceW(module, MAKEINTRESOURCEW(resource_id), resource_type);
    if (hres_info == nullptr)
    {
        PLOG(ERROR) << "FindResourceW failed";
        return false;
    }

    DWORD data_size = SizeofResource(module, hres_info);
    HGLOBAL hres = LoadResource(module, hres_info);
    if (!hres)
    {
        PLOG(ERROR) << "LoadResource failed";
        return false;
    }

    void* resource = LockResource(hres);
    if (!resource)
    {
        PLOG(ERROR) << "LockResource failed";
        return false;
    }

    *data = resource;
    *length = static_cast<size_t>(data_size);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool dataResourceFromModule(HMODULE module, int resource_id, void** data, size_t* length)
{
    return resourceFromModule(module, resource_id, L"BINDATA", data, length);
}

} // namespace base
