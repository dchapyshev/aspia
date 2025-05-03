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

#include "base/strings/string_number_conversions.h"

#include "base/logging.h"

#include <limits>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <typename T, typename std::enable_if<std::is_signed<T>::value>::type* = nullptr>
bool isValueNegative(T value)
{
    static_assert(std::is_arithmetic<T>::value, "Argument must be numeric");
    return value < 0;
}

//--------------------------------------------------------------------------------------------------
template <typename T, typename std::enable_if<!std::is_signed<T>::value>::type* = nullptr>
bool isValueNegative(T)
{
    static_assert(std::is_arithmetic<T>::value, "Argument must be numeric");
    return false;
}

//--------------------------------------------------------------------------------------------------
template <typename T>
typename std::make_unsigned<T>::type unsignedAbs(T value)
{
    static_assert(std::is_integral<T>::value, "Type must be integral");

    using UnsignedT = typename std::make_unsigned<T>::type;

    return isValueNegative(value) ?
        0 - static_cast<UnsignedT>(value) : static_cast<UnsignedT>(value);
}

//--------------------------------------------------------------------------------------------------
template <typename STR, typename INT>
struct IntToStringT
{
    static STR IntToString(INT value)
    {
        // log10(2) ~= 0.3 bytes needed per bit or per byte log10(2**8) ~= 2.4.
        // So round up to allocate 3 output characters per byte, plus 1 for '-'.
        const size_t kOutputBufSize = 3 * sizeof(INT) + std::numeric_limits<INT>::is_signed;

        // Create the string in a temporary buffer, write it back to front, and
        // then return the substr of what we ended up using.
        using CHR = typename STR::value_type;
        CHR outbuf[kOutputBufSize];

        // The ValueOrDie call below can never fail, because UnsignedAbs is valid
        // for all valid inputs.
        typename std::make_unsigned<INT>::type res = unsignedAbs(value);

        CHR* end = outbuf + kOutputBufSize;
        CHR* i = end;
        do
        {
            --i;
            DCHECK(i != outbuf);
            *i = static_cast<CHR>((res % 10) + '0');
            res /= 10;
        }
        while (res != 0);
        if (isValueNegative(value))
        {
            --i;
            DCHECK(i != outbuf);
            *i = static_cast<CHR>('-');
        }
        return STR(i, end);
    }
};

// Utility to convert a character to a digit in a given base
template<typename CHAR, int BASE, bool BASE_LTE_10>
class BaseCharToDigit
{
    // Nothing
};

// Faster specialization for bases <= 10
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, true>
{
public:
    static bool Convert(CHAR c, quint8* digit)
    {
        if (c >= '0' && c < '0' + BASE)
        {
            *digit = static_cast<quint8>(c - '0');
            return true;
        }
        return false;
    }
};

// Specialization for bases where 10 < base <= 36
template<typename CHAR, int BASE> class BaseCharToDigit<CHAR, BASE, false>
{
public:
    static bool Convert(CHAR c, quint8* digit)
    {
        if (c >= '0' && c <= '9')
        {
            *digit = c - '0';
        }
        else if (c >= 'a' && c < 'a' + BASE - 10)
        {
            *digit = c - 'a' + 10;
        }
        else if (c >= 'A' && c < 'A' + BASE - 10)
        {
            *digit = c - 'A' + 10;
        }
        else
        {
            return false;
        }
        return true;
    }
};

//--------------------------------------------------------------------------------------------------
template <int BASE, typename CHAR>
bool CharToDigit(CHAR c, quint8* digit)
{
    return BaseCharToDigit<CHAR, BASE, BASE <= 10>::Convert(c, digit);
}

// There is an IsUnicodeWhitespace for wchars defined in string_util.h, but it
// is locale independent, whereas the functions we are replacing were
// locale-dependent. TBD what is desired, but for the moment let's not
// introduce a change in behaviour.
template<typename CHAR> class WhitespaceHelper
{
    // Nothing
};

template<> class WhitespaceHelper<char>
{
public:
    static bool Invoke(char c)
    {
        return 0 != isspace(static_cast<unsigned char>(c));
    }
};

template<> class WhitespaceHelper<char16_t>
{
public:
    static bool Invoke(char16_t c)
    {
        return 0 != iswspace(c);
    }
};

template<typename CHAR> bool LocalIsWhitespace(CHAR c)
{
    return WhitespaceHelper<CHAR>::Invoke(c);
}

// IteratorRangeToNumberTraits should provide:
//  - a typedef for iterator_type, the iterator type used as input.
//  - a typedef for value_type, the target numeric type.
//  - static functions min, max (returning the minimum and maximum permitted
//    values)
//  - constant kBase, the base in which to interpret the input
template<typename IteratorRangeToNumberTraits>
class IteratorRangeToNumber
{
public:
    typedef IteratorRangeToNumberTraits traits;
    typedef typename traits::iterator_type const_iterator;
    typedef typename traits::value_type value_type;

    // Generalized iterator-range-to-number conversion.
    //
    static bool Invoke(const_iterator begin, const_iterator end, value_type* output)
    {
        bool valid = true;

        while (begin != end && LocalIsWhitespace(*begin))
        {
            valid = false;
            ++begin;
        }

        if (begin != end && *begin == '-')
        {
            if (!std::numeric_limits<value_type>::is_signed)
            {
                *output = 0;
                valid = false;
            }
            else if (!Negative::Invoke(begin + 1, end, output))
            {
                valid = false;
            }
        }
        else
        {
            if (begin != end && *begin == '+')
            {
                ++begin;
            }

            if (!Positive::Invoke(begin, end, output))
            {
                valid = false;
            }
        }

        return valid;
    }

private:
    // Sign provides:
    //  - a static function, CheckBounds, that determines whether the next digit
    //    causes an overflow/underflow
    //  - a static function, Increment, that appends the next digit appropriately
    //    according to the sign of the number being parsed.
    template<typename Sign>
    class Base
    {
    public:
        static bool Invoke(const_iterator begin, const_iterator end,
            typename traits::value_type* output)
        {
            *output = 0;

            if (begin == end)
            {
                return false;
            }

            // Note: no performance difference was found when using template
            // specialization to remove this check in bases other than 16
            if (traits::kBase == 16 && end - begin > 2 && *begin == '0' &&
                (*(begin + 1) == 'x' || *(begin + 1) == 'X'))
            {
                begin += 2;
            }

            for (const_iterator current = begin; current != end; ++current)
            {
                quint8 new_digit = 0;

                if (!CharToDigit<traits::kBase>(*current, &new_digit))
                {
                    return false;
                }

                if (current != begin)
                {
                    if (!Sign::CheckBounds(output, new_digit))
                    {
                        return false;
                    }
                    *output *= traits::kBase;
                }

                Sign::Increment(new_digit, output);
            }
            return true;
        }
    };

    class Positive : public Base<Positive>
    {
    public:
        static bool CheckBounds(value_type* output, quint8 new_digit)
        {
            if (*output > static_cast<value_type>(traits::max() / traits::kBase) ||
                (*output == static_cast<value_type>(traits::max() / traits::kBase) &&
                    new_digit > traits::max() % traits::kBase))
            {
                *output = traits::max();
                return false;
            }
            return true;
        }
        static void Increment(quint8 increment, value_type* output)
        {
            *output += increment;
        }
    };

    class Negative : public Base<Negative>
    {
    public:
        static bool CheckBounds(value_type* output, quint8 new_digit)
        {
            if (*output < traits::min() / traits::kBase ||
                (*output == traits::min() / traits::kBase &&
                    new_digit > 0 - traits::min() % traits::kBase))
            {
                *output = traits::min();
                return false;
            }
            return true;
        }
        static void Increment(quint8 increment, value_type* output)
        {
            *output -= increment;
        }
    };
};

template<typename ITERATOR, typename VALUE, int BASE>
class BaseIteratorRangeToNumberTraits
{
public:
    typedef ITERATOR iterator_type;
    typedef VALUE value_type;
    static value_type min()
    {
        return std::numeric_limits<value_type>::min();
    }
    static value_type max()
    {
        return std::numeric_limits<value_type>::max();
    }
    static const int kBase = BASE;
};

template <typename VALUE, int BASE>
class StringPieceToNumberTraits
    : public BaseIteratorRangeToNumberTraits<std::string_view::const_iterator, VALUE, BASE>
{
    // Nothing
};

//--------------------------------------------------------------------------------------------------
template <typename VALUE>
bool stringToIntImpl(std::string_view input, VALUE* output)
{
    return IteratorRangeToNumber<StringPieceToNumberTraits<VALUE, 10> >::Invoke(
        input.begin(), input.end(), output);
}

//--------------------------------------------------------------------------------------------------
template <typename VALUE, int BASE>
class StringPiece16ToNumberTraits
    : public BaseIteratorRangeToNumberTraits<std::u16string_view::const_iterator, VALUE, BASE>
{
    // Nothing
};

//--------------------------------------------------------------------------------------------------
template <typename VALUE>
bool string16ToIntImpl(std::u16string_view input, VALUE* output)
{
    return IteratorRangeToNumber<StringPiece16ToNumberTraits<VALUE, 10> >::Invoke(
        input.begin(), input.end(), output);
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool stringToInt(std::string_view input, int* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToInt(std::u16string_view input, int* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUint(std::string_view input, unsigned int* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUint(std::u16string_view input, unsigned int* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToChar(std::string_view input, signed char* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToChar(std::u16string_view input, signed char* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToShort(std::string_view input, short* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToShort(std::u16string_view input, short* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUChar(std::string_view input, unsigned char* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUChar(std::u16string_view input, unsigned char* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUShort(std::string_view input, unsigned short* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUShort(std::u16string_view input, unsigned short* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToULong(std::string_view input, unsigned long* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToULong(std::u16string_view input, unsigned long* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToInt64(std::string_view input, qint64* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToInt64(std::u16string_view input, qint64* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUint64(std::string_view input, unsigned long int* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToUint64(std::u16string_view input, unsigned long int* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToULong64(std::string_view input, unsigned long long* output)
{
    return stringToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
bool stringToULong64(std::u16string_view input, unsigned long long* output)
{
    return string16ToIntImpl(input, output);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(int value)
{
    return IntToStringT<std::string, int>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(int value)
{
    return IntToStringT<std::u16string, int>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(unsigned int value)
{
    return IntToStringT<std::string, unsigned int>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(unsigned int value)
{
    return IntToStringT<std::u16string, unsigned int>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(long value)
{
    return IntToStringT<std::string, long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(long value)
{
    return IntToStringT<std::u16string, long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(signed char value)
{
    return IntToStringT<std::string, signed char>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(signed char value)
{
    return IntToStringT<std::u16string, signed char>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(short value)
{
    return IntToStringT<std::string, short>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(short value)
{
    return IntToStringT<std::u16string, short>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(unsigned char value)
{
    return IntToStringT<std::string, unsigned char>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(unsigned char value)
{
    return IntToStringT<std::u16string, unsigned char>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(unsigned short value)
{
    return IntToStringT<std::string, unsigned short>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(unsigned short value)
{
    return IntToStringT<std::u16string, unsigned short>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(unsigned long value)
{
    return IntToStringT<std::string, unsigned long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(unsigned long value)
{
    return IntToStringT<std::u16string, unsigned long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(long long value)
{
    return IntToStringT<std::string, long long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(long long value)
{
    return IntToStringT<std::u16string, long long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::string numberToString(unsigned long long value)
{
    return IntToStringT<std::string, unsigned long long>::IntToString(value);
}

//--------------------------------------------------------------------------------------------------
std::u16string numberToString16(unsigned long long value)
{
    return IntToStringT<std::u16string, unsigned long long>::IntToString(value);
}

} // namespace base
