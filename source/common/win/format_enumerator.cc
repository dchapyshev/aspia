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

#include "common/win/format_enumerator.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FormatEnumerator::FormatEnumerator(const QVector<FORMATETC>& formats)
    : ref_count_(1),
      formats_(formats)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FormatEnumerator::~FormatEnumerator() = default;

//--------------------------------------------------------------------------------------------------
HRESULT FormatEnumerator::QueryInterface(const IID& iid, void** object)
{
    if (object == nullptr)
        return E_POINTER;

    if (iid == IID_IEnumFORMATETC || iid == IID_IUnknown)
    {
        *object = static_cast<IEnumFORMATETC*>(this);
        AddRef();
        return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
}

//--------------------------------------------------------------------------------------------------
ULONG FormatEnumerator::AddRef()
{
    ULONG new_ref = InterlockedIncrement(&ref_count_);
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
ULONG FormatEnumerator::Release()
{
    ULONG new_ref = InterlockedDecrement(&ref_count_);
    if (new_ref == 0)
        delete this;
    return new_ref;
}

//--------------------------------------------------------------------------------------------------
HRESULT FormatEnumerator::Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched)
{
    ULONG fetched = 0;

    while (fetched < celt && index_ < formats_.size())
    {
        rgelt[fetched++] = formats_[index_++];
    }

    if (pceltFetched)
        *pceltFetched = fetched;

    return (fetched == celt) ? S_OK : S_FALSE;
}

//--------------------------------------------------------------------------------------------------
HRESULT FormatEnumerator::Skip(ULONG celt)
{
    index_ += celt;

    if (index_ > formats_.size())
        index_ = formats_.size();

    return (index_ < formats_.size()) ? S_OK : S_FALSE;
}

//--------------------------------------------------------------------------------------------------
HRESULT FormatEnumerator::Reset()
{
    index_ = 0;
    return S_OK;
}

//--------------------------------------------------------------------------------------------------
HRESULT FormatEnumerator::Clone(IEnumFORMATETC** ppEnum)
{
    *ppEnum = new FormatEnumerator(formats_);
    return S_OK;
}

} // namespace common
