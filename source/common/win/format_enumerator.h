//
// SmartCafe Project
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

#ifndef COMMON_WIN_FORMAT_ENUMERATOR_H
#define COMMON_WIN_FORMAT_ENUMERATOR_H

#include <QVector>

#include <shlobj.h>

namespace common {

class FormatEnumerator final : public IEnumFORMATETC
{
public:
    explicit FormatEnumerator(const QVector<FORMATETC>& formats);
    virtual ~FormatEnumerator() final;

private:
    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID iid, void** object) final;
    ULONG __stdcall AddRef() final;
    ULONG __stdcall Release() final;

    // IEnumFORMATETC
    HRESULT __stdcall Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) final;
    HRESULT __stdcall Skip(ULONG celt) final;
    HRESULT __stdcall Reset() final;
    HRESULT __stdcall Clone(IEnumFORMATETC** ppEnum) final;

    LONG ref_count_ = 1;
    const QVector<FORMATETC> formats_;
    qsizetype index_;
};

} // namespace common

#endif // COMMON_WIN_FORMAT_ENUMERATOR_H
