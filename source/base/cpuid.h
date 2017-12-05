//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/cpuid.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__CPUID_H
#define _ASPIA_BASE__CPUID_H

#include <intrin.h>
#include <cstdint>

namespace aspia {

class CPUID
{
public:
    CPUID() = default;
    CPUID(int leaf) { get(leaf); }
    CPUID(int leaf, int subleaf) { get(leaf, subleaf); }
    virtual ~CPUID() = default;

    CPUID(const CPUID& other)
    {
        memcpy(cpu_info_, other.cpu_info_, sizeof(cpu_info_));
    }

    void get(int leaf)
    {
        __cpuid(cpu_info_, leaf);
    }

    void get(int leaf, int subleaf)
    {
        __cpuidex(cpu_info_, leaf, subleaf);
    }

    uint32_t eax() const { return static_cast<uint32_t>(cpu_info_[kEAX]); }
    uint32_t ebx() const { return static_cast<uint32_t>(cpu_info_[kEBX]); }
    uint32_t ecx() const { return static_cast<uint32_t>(cpu_info_[kECX]); }
    uint32_t edx() const { return static_cast<uint32_t>(cpu_info_[kEDX]); }

    CPUID& operator=(const CPUID& other)
    {
        memcpy(cpu_info_, other.cpu_info_, sizeof(cpu_info_));
        return *this;
    }

private:
    static constexpr int kEAX = 0;
    static constexpr int kEBX = 1;
    static constexpr int kECX = 2;
    static constexpr int kEDX = 3;

    int cpu_info_[4] = { -1 };
};

} // namespace aspia

#endif // _ASPIA_BASE__CPUID_H
