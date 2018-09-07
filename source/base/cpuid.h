//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__CPUID_H_
#define ASPIA_BASE__CPUID_H_

#include <cstdint>

namespace aspia {

class CPUID
{
public:
    CPUID() = default;
    explicit CPUID(int leaf) { get(leaf); }
    CPUID(int leaf, int subleaf) { get(leaf, subleaf); }
    virtual ~CPUID() = default;

    CPUID(const CPUID& other);
    CPUID& operator=(const CPUID& other);

    void get(int leaf);
    void get(int leaf, int subleaf);

    uint32_t eax() const { return static_cast<uint32_t>(cpu_info_[kEAX]); }
    uint32_t ebx() const { return static_cast<uint32_t>(cpu_info_[kEBX]); }
    uint32_t ecx() const { return static_cast<uint32_t>(cpu_info_[kECX]); }
    uint32_t edx() const { return static_cast<uint32_t>(cpu_info_[kEDX]); }

    static bool hasAesNi();

private:
    static constexpr int kEAX = 0;
    static constexpr int kEBX = 1;
    static constexpr int kECX = 2;
    static constexpr int kEDX = 3;

    int cpu_info_[4] = { -1 };
};

} // namespace aspia

#endif // ASPIA_BASE__CPUID_H_
