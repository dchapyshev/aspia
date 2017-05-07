//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/scoped_bstr.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/scoped_bstr.h"

#include "base/logging.h"

namespace aspia {

ScopedBstr::ScopedBstr(const WCHAR* non_bstr) :
    bstr_(SysAllocString(non_bstr))
{
    // Nothing
}

ScopedBstr::~ScopedBstr()
{
    static_assert(sizeof(ScopedBstr) == sizeof(BSTR), "ScopedBstrSize");
    SysFreeString(bstr_);
}

void ScopedBstr::Reset(BSTR bstr)
{
    if (bstr != bstr_)
    {
        // if |bstr_| is NULL, SysFreeString does nothing.
        SysFreeString(bstr_);
        bstr_ = bstr;
    }
}

BSTR ScopedBstr::Release()
{
    BSTR bstr = bstr_;
    bstr_ = nullptr;
    return bstr;
}

void ScopedBstr::Swap(ScopedBstr& bstr2)
{
    BSTR tmp = bstr_;
    bstr_ = bstr2.bstr_;
    bstr2.bstr_ = tmp;
}

BSTR* ScopedBstr::Receive()
{
    DCHECK(!bstr_) << "BSTR leak.";
    return &bstr_;
}

BSTR ScopedBstr::Allocate(const WCHAR* str)
{
    Reset(SysAllocString(str));
    return bstr_;
}

BSTR ScopedBstr::AllocateBytes(size_t bytes)
{
    Reset(SysAllocStringByteLen(nullptr, static_cast<UINT>(bytes)));
    return bstr_;
}

void ScopedBstr::SetByteLen(size_t bytes)
{
    DCHECK(bstr_ != nullptr) << "attempting to modify a NULL bstr";
    uint32_t *data = reinterpret_cast<uint32_t*>(bstr_);
    data[-1] = static_cast<uint32_t>(bytes);
}

size_t ScopedBstr::Length() const
{
    return SysStringLen(bstr_);
}

size_t ScopedBstr::ByteLength() const
{
    return SysStringByteLen(bstr_);
}

}  // namespace aspia
