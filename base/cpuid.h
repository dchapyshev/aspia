/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/cpuid.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__CPUID_H
#define _ASPIA_BASE__CPUID_H

#include <stdint.h>
#include <intrin.h>

class CPUID
{
public:
    CPUID(int leaf)
    {
        cpu_info_[0] = cpu_info_[1] = cpu_info_[2] = cpu_info_[3] = -1;
        ::__cpuid(cpu_info_, leaf);
    }

    CPUID(int leaf, int subleaf)
    {
        cpu_info_[0] = cpu_info_[1] = cpu_info_[2] = cpu_info_[3] = -1;
        ::__cpuidex(cpu_info_, leaf, subleaf);
    }

    ~CPUID() {}

    uint32_t eax() const
    {
        return static_cast<uint32_t>(cpu_info_[0]);
    }

    uint32_t eax(int from, int to) const
    {
        return bits<uint32_t>(eax(), from, to);
    }

    uint32_t eax(int num) const
    {
        return bit<uint32_t>(eax(), num);
    }

    uint32_t ebx() const
    {
        return static_cast<uint32_t>(cpu_info_[1]);
    }

    uint32_t ebx(int from, int to) const
    {
        return bits<uint32_t>(ebx(), from, to);
    }

    uint32_t ebx(int num) const
    {
        return bit<uint32_t>(ebx(), num);
    }

    uint32_t ecx() const
    {
        return static_cast<uint32_t>(cpu_info_[2]);
    }

    uint32_t ecx(int from, int to) const
    {
        return bits<uint32_t>(ecx(), from, to);
    }

    uint32_t ecx(int num) const
    {
        return bit<uint32_t>(ecx(), num);
    }

    uint32_t edx() const
    {
        return static_cast<uint32_t>(cpu_info_[3]);
    }

    uint32_t edx(int from, int to) const
    {
        return bits<uint32_t>(edx(), from, to);
    }

    uint32_t edx(int num) const
    {
        return bit<uint32_t>(edx(), num);
    }

private:
    template <typename Type>
    static inline Type bits(Type val, int from, int to)
    {
        Type mask = (static_cast<Type>(1) << (to + 1)) - 1;
        return ((val & mask) >> from);
    }

    template <typename Type>
    static inline Type bit(Type val, int num)
    {
        return bits<Type>(val, num, num);
    }

private:
    int cpu_info_[4];
};

#endif // _ASPIA_BASE__CPUID_H
