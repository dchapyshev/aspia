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

#ifndef BASE_AUDIO_LINUX_LATE_BINDING_SYMBOL_TABLE_H
#define BASE_AUDIO_LINUX_LATE_BINDING_SYMBOL_TABLE_H

#include "base/macros_magic.h"

#include <cassert>
#include <cstring>

namespace base {

using DllHandle = void*;
const DllHandle kInvalidDllHandle = nullptr;

// These are helpers for use only by the class below.
DllHandle internalLoadDll(const char dll_name[]);
void internalUnloadDll(DllHandle handle);
bool internalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char* const symbol_names[],
                         void* symbols[]);

template <int SYMBOL_TABLE_SIZE, const char kDllName[], const char* const kSymbolNames[]>
class LateBindingSymbolTable
{
public:
    LateBindingSymbolTable()
        : handle_(kInvalidDllHandle),
          undefined_symbols_(false)
    {
        memset(symbols_, 0, sizeof(symbols_));
    }

    ~LateBindingSymbolTable() { unload(); }

    static int numSymbols() { return SYMBOL_TABLE_SIZE; }

    // We do not use this, but we offer it for theoretical convenience.
    static const char* symbolName(int index)
    {
        assert(index < numSymbols());
        return kSymbolNames[index];
    }

    bool isLoaded() const { return handle_ != kInvalidDllHandle; }

    // Loads the DLL and the symbol table. Returns true if the DLL and symbol table loaded
    // successfully.
    bool load()
    {
        if (isLoaded())
            return true;

        if (undefined_symbols_)
        {
            // We do not attempt to load again because repeated attempts are not
            // likely to succeed and DLL loading is costly.
            return false;
        }

        handle_ = internalLoadDll(kDllName);
        if (!isLoaded())
            return false;

        if (!internalLoadSymbols(handle_, numSymbols(), kSymbolNames, symbols_))
        {
            undefined_symbols_ = true;
            unload();
            return false;
        }
        return true;
    }

    void unload()
    {
        if (!isLoaded())
            return;

        internalUnloadDll(handle_);
        handle_ = kInvalidDllHandle;
        memset(symbols_, 0, sizeof(symbols_));
    }

    // Retrieves the given symbol. NOTE: Recommended to use LATESYM_GET below instead of this.
    void* symbol(int index) const
    {
        assert(isLoaded());
        assert(index < numSymbols());
        return symbols_[index];
    }

private:
    DllHandle handle_;
    bool undefined_symbols_;
    void* symbols_[SYMBOL_TABLE_SIZE];

    Q_DISABLE_COPY(LateBindingSymbolTable)
};

// This macro must be invoked in a header to declare a symbol table class.
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(ClassName) enum {

// This macro must be invoked in the header declaration once for each symbol (recommended to use an
// X-Macro to avoid duplication). This macro defines an enum with names built from the symbols, which
// essentially creates a hash table in the compiler from symbol names to their indices in the symbol
// table class.
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(ClassName, sym)   \
    ClassName##_SYMBOL_TABLE_INDEX_##sym,

// This macro completes the header declaration.
#define LATE_BINDING_SYMBOL_TABLE_DECLARE_END(ClassName)          \
    ClassName##_SYMBOL_TABLE_SIZE                                 \
    };                                                            \
                                                                  \
    extern const char ClassName##_kDllName[];                     \
    extern const char* const                                      \
        ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE];  \
                                                                  \
    typedef ::base::LateBindingSymbolTable<                       \
        ClassName##_SYMBOL_TABLE_SIZE, ClassName##_kDllName,      \
        ClassName##_kSymbolNames>                                 \
        ClassName;

// This macro must be invoked in a .cc file to define a previously-declared symbol table class.
#define LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(ClassName, dllName) \
    const char ClassName##_kDllName[] = dllName;                   \
    const char* const ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE] = {

// This macro must be invoked in the .cc definition once for each symbol (recommended to use an
// X-Macro to avoid duplication).
// This would have to use the mangled name if we were to ever support C++ symbols.
#define LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(ClassName, sym) #sym,

#define LATE_BINDING_SYMBOL_TABLE_DEFINE_END(ClassName)   };

// Index of a given symbol in the given symbol table class.
#define LATESYM_INDEXOF(ClassName, sym) (ClassName##_SYMBOL_TABLE_INDEX_##sym)

// Returns a reference to the given late-binded symbol, with the correct type.
#define LATESYM_GET(ClassName, inst, sym) \
    (*reinterpret_cast<__typeof__(&sym)>((inst)->symbol(LATESYM_INDEXOF(ClassName, sym))))

} // namespace base

#endif // BASE_AUDIO_LINUX_LATE_BINDING_SYMBOL_TABLE_H
