//
// PROJECT:         Aspia
// FILE:            base/bitset.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__BITSET_H
#define _ASPIA_BASE__BITSET_H

namespace aspia {

template<typename NumericType>
class BitSet
{
public:
    BitSet() = default;
    BitSet(NumericType value) : value_(value) { /* Nothing */ }
    BitSet(const BitSet& other) { copyFrom(other); }
    virtual ~BitSet() = default;

    static constexpr size_t kBitsPerByte = 8;
    static constexpr NumericType kMaxValue = std::numeric_limits<NumericType>::max();

    // Returns the value of the bit at the position |pos|.
    bool test(size_t pos) const { return !!range(pos, pos); }

    // Returns the value of the bit range at the range from |from| to |to|.
    NumericType range(size_t from, size_t to) const
    {
        assert(from <= to && from < size() && to < size());
        const NumericType mask = (static_cast<NumericType>(1) << (to + 1)) - 1;
        return ((value_ & mask) >> from);
    }

    // Checks if all bits are set to true.
    bool all() const { return value_ == kMaxValue; }

    // Checks if none of the bits are set to true.
    bool none() const { return value_ == 0; }

    // Checks if any bits are set to true.
    bool any() const { return value_ != 0; }

    // Sets all bits to true.
    BitSet& set()
    {
        value_ = kMaxValue;
        return *this;
    }

    // Sets the bit at position |pos| to the value |value|.
    BitSet& set(size_t pos, bool value = true)
    {
        assert(pos < size());

        const NumericType mask = static_cast<NumericType>(1) << pos;

        if (value)
            value_ |= mask;
        else
            value_ &= ~mask;

        return *this;
    }

    // Sets all bits to false.
    BitSet& reset()
    {
        value_ = 0;
        return *this;
    }

    // Sets the bit at position |pos| to false.
    BitSet& reset(size_t pos) { return set(pos, false); }

    // Flips all bits.
    BitSet& flip()
    {
        value_ = ~value_;
        return *this;
    }

    // Flips the bit at the position |pos|.
    BitSet& flip(size_t pos)
    {
        assert(pos < Size());

        value_ ^= static_cast<NumericType>(1) << pos;
        return *this;
    }

    // Returns the number of bits that are set to true.
    size_t count() const
    {
        size_t counter = 0;

        for (size_t pos = 0; pos < size(); ++pos)
        {
            if (test(pos))
                ++counter;
        }

        return count;
    }

    // Returns numberic value of bitset.
    NumericType value() const { return value_; }

    // Copies a value.
    void copyFrom(NumericType value) { value_ = value; }
    void copyFrom(const BitSet& other) { value_ = other.value_; }

    // Returns the number of bits that the bitset can hold.
    static size_t size() { return sizeof(NumericType) * kBitsPerByte; }

    // Returns true if all of the bits in *this and |other| are equal.
    bool operator==(const BitSet& other) const { return value_ == other.value_; }

    // Returns true if any of the bits in *this and |other| are not equal.
    bool operator!=(const BitSet& other) const { return value_ != other.value_; }

    BitSet& operator=(const BitSet& other)
    {
        copyFrom(other);
        return *this;
    }

private:
    NumericType value_ = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__BITSET_H
