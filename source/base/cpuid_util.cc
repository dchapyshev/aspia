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

#include "base/cpuid_util.h"

#if defined(Q_PROCESSOR_X86)

#include "base/bitset.h"

#include <algorithm>

#if defined(Q_CC_MSVC)
#include <intrin.h>
#else
#include <cpuid.h>
#endif // Q_CC_MSVC

namespace {

// Leaves the vendors have defined so far end well below these. A processor answering something
// else for the maximum leaf is not followed there.
constexpr quint32 kMaxBasicLeaf = 0x40;
constexpr quint32 kMaxHypervisorLeaf = 0x40000010;
constexpr quint32 kMaxExtendedLeaf = 0x80000040;
constexpr quint32 kMaxCentaurLeaf = 0xC0000010;

// Leaves with subleaves are asked until they say there is nothing more, and this is where the
// asking stops in any case.
constexpr quint32 kMaxSubleafs = 32;

//--------------------------------------------------------------------------------------------------
void addLeaf(QList<CpuidUtil::Leaf>* leafs, quint32 leaf, quint32 subleaf)
{
    const CpuidUtil cpuid(static_cast<int>(leaf), static_cast<int>(subleaf));

    CpuidUtil::Leaf item;

    item.leaf = leaf;
    item.subleaf = subleaf;
    item.eax = cpuid.eax();
    item.ebx = cpuid.ebx();
    item.ecx = cpuid.ecx();
    item.edx = cpuid.edx();

    leafs->append(item);
}

//--------------------------------------------------------------------------------------------------
void addSubleafs(QList<CpuidUtil::Leaf>* leafs, quint32 leaf)
{
    switch (leaf)
    {
        // Cache leaves list the caches one by one and end with a subleaf that reports none. The
        // first subleaf is kept even then: a leaf the processor does not use still belongs in the
        // dump.
        case 0x00000004:
        case 0x8000001D:
        {
            for (quint32 subleaf = 0; subleaf < kMaxSubleafs; ++subleaf)
            {
                if (subleaf &&
                    !(CpuidUtil(static_cast<int>(leaf), static_cast<int>(subleaf)).eax() & 0x1F))
                {
                    break;
                }

                addLeaf(leafs, leaf, subleaf);
            }
        }
        break;

        // Topology leaves end with a subleaf that reports no level.
        case 0x0000000B:
        case 0x0000001F:
        case 0x80000026:
        {
            for (quint32 subleaf = 0; subleaf < kMaxSubleafs; ++subleaf)
            {
                const CpuidUtil cpuid(static_cast<int>(leaf), static_cast<int>(subleaf));

                if (subleaf && !((cpuid.ecx() >> 8) & 0xFF))
                    break;

                addLeaf(leafs, leaf, subleaf);
            }
        }
        break;

        // Leaves whose first subleaf tells how many more of them there are.
        case 0x00000007:
        case 0x00000014:
        case 0x00000017:
        case 0x00000018:
        case 0x0000001D:
        case 0x00000020:
        case 0x00000023:
        case 0x00000024:
        {
            const quint32 last = std::min(CpuidUtil(static_cast<int>(leaf)).eax(),
                                          kMaxSubleafs - 1);

            for (quint32 subleaf = 0; subleaf <= last; ++subleaf)
                addLeaf(leafs, leaf, subleaf);
        }
        break;

        // The state components of the extended state leaf: the first two subleaves are always
        // there, the rest only when the processor supports the component.
        case 0x0000000D:
        {
            addLeaf(leafs, leaf, 0);
            addLeaf(leafs, leaf, 1);

            for (quint32 subleaf = 2; subleaf < kMaxSubleafs; ++subleaf)
            {
                const CpuidUtil cpuid(static_cast<int>(leaf), static_cast<int>(subleaf));

                if (cpuid.eax() || cpuid.ebx() || cpuid.ecx() || cpuid.edx())
                    addLeaf(leafs, leaf, subleaf);
            }
        }
        break;

        // Leaves with a fixed and small number of subleaves.
        case 0x0000000F:
        case 0x00000010:
        case 0x00000012:
        {
            for (quint32 subleaf = 0; subleaf < 4; ++subleaf)
            {
                const CpuidUtil cpuid(static_cast<int>(leaf), static_cast<int>(subleaf));

                if (!subleaf || cpuid.eax() || cpuid.ebx() || cpuid.ecx() || cpuid.edx())
                    addLeaf(leafs, leaf, subleaf);
            }
        }
        break;

        default:
            addLeaf(leafs, leaf, 0);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void addLeafs(QList<CpuidUtil::Leaf>* leafs, quint32 first_leaf, quint32 last_leaf)
{
    for (quint32 leaf = first_leaf; leaf <= last_leaf; ++leaf)
        addSubleafs(leafs, leaf);
}

} // namespace

//--------------------------------------------------------------------------------------------------
CpuidUtil::CpuidUtil(int leaf, int subleaf)
{
    get(leaf, subleaf);
}

//--------------------------------------------------------------------------------------------------
CpuidUtil::CpuidUtil(const CpuidUtil& other)
{
    *this = other;
}

//--------------------------------------------------------------------------------------------------
CpuidUtil& CpuidUtil::operator=(const CpuidUtil& other)
{
    if (&other == this)
        return *this;

    eax_ = other.eax_;
    ebx_ = other.ebx_;
    ecx_ = other.ecx_;
    edx_ = other.edx_;
    return *this;
}

//--------------------------------------------------------------------------------------------------
void CpuidUtil::get(int leaf, int subleaf)
{
#if defined(Q_CC_MSVC)
    int cpu_info[4];
    __cpuidex(cpu_info, leaf, subleaf);
#else
    unsigned int cpu_info[4] = { 0, 0, 0, 0 };
    __cpuid_count(leaf, subleaf, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif

    eax_ = static_cast<quint32>(cpu_info[0]);
    ebx_ = static_cast<quint32>(cpu_info[1]);
    ecx_ = static_cast<quint32>(cpu_info[2]);
    edx_ = static_cast<quint32>(cpu_info[3]);
}

//--------------------------------------------------------------------------------------------------
// static
QList<CpuidUtil::Leaf> CpuidUtil::dump()
{
    QList<Leaf> leafs;

    const quint32 max_basic = CpuidUtil(0).eax();
    if (!max_basic)
        return leafs;

    addLeafs(&leafs, 0x00000000, std::min(max_basic, kMaxBasicLeaf));

    // The leaves of a hypervisor lie above the maximum leaf of the processor and are only defined
    // when the processor says it runs under one.
    if (CpuidUtil(1).ecx() & (1u << 31))
    {
        const quint32 max_hypervisor = CpuidUtil(0x40000000).eax();

        // Not every hypervisor fills in the leaf it supports up to.
        if (max_hypervisor > 0x40000000)
            addLeafs(&leafs, 0x40000000, std::min(max_hypervisor, kMaxHypervisorLeaf));
        else
            addLeafs(&leafs, 0x40000000, 0x40000000);
    }

    const quint32 max_extended = CpuidUtil(static_cast<int>(0x80000000)).eax();
    if (max_extended > 0x80000000)
        addLeafs(&leafs, 0x80000000, std::min(max_extended, kMaxExtendedLeaf));

    // Centaur processors answer in a range of their own. No other processor places its maximum
    // leaf there, so a value out of the range means there is no range.
    const quint32 max_centaur = CpuidUtil(static_cast<int>(0xC0000000)).eax();
    if (max_centaur > 0xC0000000 && max_centaur < 0xC0001000)
        addLeafs(&leafs, 0xC0000000, std::min(max_centaur, kMaxCentaurLeaf));

    return leafs;
}

//--------------------------------------------------------------------------------------------------
// static
bool CpuidUtil::hasAesNi()
{
    // Check if function 1 is supported.
    if (CpuidUtil(0).eax() < 1)
        return false;

    // Bit 25 of register ECX set to 1 indicates the support of AES instructions.
    return BitSet<quint32>(CpuidUtil(1).ecx()).test(25);
}

#endif // defined(Q_PROCESSOR_X86)
