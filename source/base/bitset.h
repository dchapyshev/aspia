//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/bitset.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__BITSET_H
#define _ASPIA_BASE__BITSET_H

#include <assert.h>
#include <cstdint>
#include <limits>

namespace aspia {

template<typename NumericType>
class BitSet
{
public:
    BitSet() = default;
    explicit BitSet(NumericType value) : value_(value) { /* Nothing */ }
    BitSet(const BitSet& other) { CopyFrom(other); }
    virtual ~BitSet() = default;

    static constexpr size_t kBitsPerByte = 8;
    static constexpr NumericType kMaxValue = std::numeric_limits<NumericType>::max();

    // Returns the value of the bit at the position |pos|.
    bool Test(size_t pos) const { return !!Range(pos, pos); }

    // Returns the value of the bit range at the range from |from| to |to|.
    NumericType Range(size_t from, size_t to) const
    {
        assert(from <= to && from < Size() && to < Size());
        const NumericType mask = (static_cast<NumericType>(1) << (to + 1)) - 1;
        return ((value_ & mask) >> from);
    }

    // Checks if all bits are set to true.
    bool All() const { return value_ == kMaxValue; }

    // Checks if none of the bits are set to true.
    bool None() const { return value_ == 0; }

    // Checks if any bits are set to true.
    bool Any() const { return value_ != 0; }

    // Sets all bits to true.
    BitSet& Set()
    {
        value_ = kMaxValue;
        return *this;
    }

    // Sets the bit at position |pos| to the value |value|.
    BitSet& Set(size_t pos, bool value = true)
    {
        assert(pos < Size());

        const NumericType mask = static_cast<NumericType>(1) << pos;

        if (value)
            value_ |= mask;
        else
            value_ &= ~mask;

        return *this;
    }

    // Sets all bits to false.
    BitSet& Reset()
    {
        value_ = 0;
        return *this;
    }

    // Sets the bit at position |pos| to false.
    BitSet& Reset(size_t pos) { return Set(pos, false); }

    // Flips all bits.
    BitSet& Flip()
    {
        value_ = ~value_;
        return *this;
    }

    // Flips the bit at the position |pos|.
    BitSet& Flip(size_t pos)
    {
        assert(pos < Size());

        value_ ^= static_cast<NumericType>(1) << pos;
        return *this;
    }

    // Returns the number of bits that are set to true.
    size_t Count() const
    {
        size_t count = 0;

        for (size_t pos = 0; pos < Size(); ++pos)
        {
            if (Test(pos))
            {
                ++count;
            }
        }

        return count;
    }

    // Returns numberic value of bitset.
    NumericType Value() const { return value_; }

    void CopyFrom(const BitSet& other) { value_ = other.value_; }

    // Returns the number of bits that the bitset can hold.
    static size_t Size() { return sizeof(NumericType) * kBitsPerByte; }

    // Returns true if all of the bits in *this and |other| are equal.
    bool operator==(const BitSet& other) const { return value_ == other.value_; }

    // Returns true if any of the bits in *this and |other| are not equal.
    bool operator!=(const BitSet& other) const { return value_ != other.value_; }

    BitSet& operator=(const BitSet& other)
    {
        CopyFrom(other);
        return *this;
    }

private:
    NumericType value_ = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__BITSET_H
