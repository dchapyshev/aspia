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

#ifndef HOST_CREDENTIALS_UTILS_H
#define HOST_CREDENTIALS_UTILS_H

#include <credentialprovider.h>
#include <windows.h>
#include <shlwapi.h>
#include <objbase.h>

#include <cwchar>
#include <string>

//--------------------------------------------------------------------------------------------------
enum FieldId
{
    SFI_LOGO       = 0,
    SFI_LABEL      = 1,
    SFI_NUM_FIELDS = 2,
};

//--------------------------------------------------------------------------------------------------
struct FieldStatePair
{
    CREDENTIAL_PROVIDER_FIELD_STATE state;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE interactive_state;
};

//--------------------------------------------------------------------------------------------------
class ScopedHandle
{
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : handle_(handle) { /* Nothing */ }
    ~ScopedHandle() { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    HANDLE get() const { return handle_; }
    bool isValid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

    void reset(HANDLE handle = nullptr)
    {
        if (isValid())
            CloseHandle(handle_);
        handle_ = handle;
    }

    HANDLE release()
    {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

private:
    HANDLE handle_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
template<typename T>
class ScopedCoMem
{
public:
    ScopedCoMem() = default;
    ~ScopedCoMem() { reset(nullptr); }

    ScopedCoMem(const ScopedCoMem&) = delete;
    ScopedCoMem& operator=(const ScopedCoMem&) = delete;

    T** operator&()
    {
        reset(nullptr);
        return &mem_ptr_;
    }

    operator T*() const { return mem_ptr_; }
    T* operator->() const { return mem_ptr_; }
    T* get() const { return mem_ptr_; }

    void reset(T* ptr = nullptr)
    {
        if (mem_ptr_)
            CoTaskMemFree(mem_ptr_);
        mem_ptr_ = ptr;
    }

    T* release()
    {
        T* ptr = mem_ptr_;
        mem_ptr_ = nullptr;
        return ptr;
    }

private:
    T* mem_ptr_ = nullptr;
};

//--------------------------------------------------------------------------------------------------
inline void secureReset(ScopedCoMem<wchar_t>& str)
{
    if (str)
        SecureZeroMemory(str.get(), wcslen(str) * sizeof(wchar_t));
    str.reset();
}

//--------------------------------------------------------------------------------------------------
inline void secureClear(std::wstring& str)
{
    if (!str.empty())
        SecureZeroMemory(&str[0], str.size() * sizeof(wchar_t));
    str.clear();
}

#endif // HOST_CREDENTIALS_UTILS_H
