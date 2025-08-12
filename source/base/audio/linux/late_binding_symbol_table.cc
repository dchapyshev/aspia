//
// Aspia Project
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

#include "base/audio/linux/late_binding_symbol_table.h"

#include "base/logging.h"

#include <dlfcn.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
const char* dllError()
{
    char* err = dlerror();
    if (err)
        return err;
    return "No error";
}

//--------------------------------------------------------------------------------------------------
bool loadSymbol(DllHandle handle, const char* symbol_name, void** symbol)
{
    *symbol = dlsym(handle, symbol_name);
    char* err = dlerror();
    if (err)
    {
        LOG(ERROR) << "Error loading symbol" << symbol_name << ":" << err;
        return false;
    }
    else if (!*symbol)
    {
        LOG(ERROR) << "Symbol" << symbol_name << "is NULL";
        return false;
    }
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DllHandle internalLoadDll(const char dll_name[])
{
    DllHandle handle = dlopen(dll_name, RTLD_NOW);

    if (handle == kInvalidDllHandle)
    {
        LOG(ERROR) << "Can't load" << dll_name << ":" << dllError();
    }
    return handle;
}

//--------------------------------------------------------------------------------------------------
void internalUnloadDll(DllHandle handle)
{
    if (dlclose(handle) != 0)
    {
        LOG(ERROR) << dllError();
    }
}

//--------------------------------------------------------------------------------------------------
bool internalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char* const symbol_names[],
                         void* symbols[])
{
    // Clear any old errors.
    dlerror();

    for (int i = 0; i < num_symbols; ++i)
    {
        if (!loadSymbol(handle, symbol_names[i], &symbols[i]))
            return false;
    }
    return true;
}

} // namespace base
