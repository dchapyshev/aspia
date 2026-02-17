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

#ifndef BASE_CPUID_UTIL_H
#define BASE_CPUID_UTIL_H

#include <QtGlobal>

#if defined(Q_PROCESSOR_X86)

#if !defined(Q_CC_MSVC)
#include <climits>
#endif

namespace base {

class CpuidUtil
{
public:
    CpuidUtil() = default;
    CpuidUtil(int leaf, int subleaf = 0);
    virtual ~CpuidUtil() = default;

    CpuidUtil(const CpuidUtil& other);
    CpuidUtil& operator=(const CpuidUtil& other);

    void get(int leaf, int subleaf = 0);

    quint32 eax() const { return eax_; }
    quint32 ebx() const { return ebx_; }
    quint32 ecx() const { return ecx_; }
    quint32 edx() const { return edx_; }

    static bool hasAesNi();

private:
    quint32 eax_ = 0;
    quint32 ebx_ = 0;
    quint32 ecx_ = 0;
    quint32 edx_ = 0;
};

} // namespace base

#endif // defined(Q_PROCESSOR_X86)
#endif // BASE_CPUID_UTIL_H
