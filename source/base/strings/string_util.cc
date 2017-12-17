//
// PROJECT:         Aspia
// FILE:            base/strings/string_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_util.h"
#include "base/strings/string_util_constants.h"
#include "base/logging.h"

#include <algorithm>
#include <cwctype>
#include <cctype>
#include <strsafe.h>

namespace aspia {

namespace {

static const std::string kEmptyString;
static const std::wstring kEmptyStringWide;

// Assuming that a pointer is the size of a "machine word", then
// uintptr_t is an integer type that is also a machine word.
using MachineWord = uintptr_t;
const uintptr_t kMachineWordAlignmentMask = sizeof(MachineWord) - 1;

bool IsAlignedToMachineWord(const void* pointer)
{
    return !(reinterpret_cast<MachineWord>(pointer) & kMachineWordAlignmentMask);
}

template<typename T> T* AlignToMachineWord(T* pointer)
{
    return reinterpret_cast<T*>(reinterpret_cast<MachineWord>(pointer) &
        ~kMachineWordAlignmentMask);
}

template<size_t size, typename CharacterType> struct NonASCIIMask;

template<> struct NonASCIIMask<4, wchar_t>
{
    static inline uint32_t value() { return 0xFF80FF80U; }
};

template<> struct NonASCIIMask<4, char>
{
    static inline uint32_t value() { return 0x80808080U; }
};

template<> struct NonASCIIMask<8, wchar_t>
{
    static inline uint64_t value() { return 0xFF80FF80FF80FF80ULL; }
};

template<> struct NonASCIIMask<8, char>
{
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};

} // namespace

std::string ReplaceLfByCrLf(const std::string& in)
{
    std::string out;

    out.resize(2 * in.size());

    char* out_p_begin = &out[0];
    char* out_p = out_p_begin;
    const char* in_p_begin = &in[0];
    const char* in_p_end = &in[in.size()];

    for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p)
    {
        char c = *in_p;

        if (c == '\n')
        {
            *out_p++ = '\r';
        }

        *out_p++ = c;
    }

    out.resize(out_p - out_p_begin);

    return out;
}

std::string ReplaceCrLfByLf(const std::string& in)
{
    std::string out;

    out.resize(in.size());

    char* out_p_begin = &out[0];
    char* out_p = out_p_begin;
    const char* in_p_begin = &in[0];
    const char* in_p_end = &in[in.size()];

    for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p)
    {
        char c = *in_p;

        if ((c == '\r') && (in_p + 1 < in_p_end) && (*(in_p + 1) == '\n'))
        {
            *out_p++ = '\n';
            ++in_p;
        }
        else
        {
            *out_p++ = c;
        }
    }

    out.resize(out_p - out_p_begin);

    return out;
}

bool IsStringUTF8(const char* data, size_t length)
{
    const char* ptr = data;
    const char* ptr_end = data + length;

    while (ptr != ptr_end)
    {
        if ((*ptr & 0x80) == 0)
        {
            // Single-byte symbol.
            ++ptr;
        }
        else if ((*ptr & 0xc0) == 0x80 || (*ptr & 0xfe) == 0xfe)
        {
            // Invalid first byte.
            return false;
        }
        else
        {
            // First byte of a multi-byte symbol. The bits from 2 to 6 are the count
            // of continuation bytes (up to 5 of them).
            for (char first = *ptr << 1; first & 0x80; first <<= 1)
            {
                ++ptr;

                // Missing continuation byte.
                if (ptr == ptr_end)
                    return false;

                // Invalid continuation byte.
                if ((*ptr & 0xc0) != 0x80)
                    return false;
            }

            ++ptr;
        }
    }

    return true;
}

bool IsStringUTF8(const std::string& string)
{
    return IsStringUTF8(string.data(), string.length());
}

template <class Char>
bool DoIsStringASCII(const Char* characters, size_t length)
{
    MachineWord all_char_bits = 0;
    const Char* end = characters + length;

    // Prologue: align the input.
    while (!IsAlignedToMachineWord(characters) && characters != end)
    {
        all_char_bits |= *characters;
        ++characters;
    }

    // Compare the values of CPU word size.
    const Char* word_end = AlignToMachineWord(end);
    const size_t loop_increment = sizeof(MachineWord) / sizeof(Char);

    while (characters < word_end)
    {
        all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
        characters += loop_increment;
    }

    // Process the remaining bytes.
    while (characters != end)
    {
        all_char_bits |= *characters;
        ++characters;
    }

    MachineWord non_ascii_bit_mask = NonASCIIMask<sizeof(MachineWord), Char>::value();

    return !(all_char_bits & non_ascii_bit_mask);
}

bool IsStringASCII(const char* data, size_t length)
{
    return DoIsStringASCII(data, length);
}

bool IsStringASCII(const std::string& string)
{
    return DoIsStringASCII(string.data(), string.length());
}

bool IsStringASCII(const wchar_t* data, size_t length)
{
    return DoIsStringASCII(data, length);
}

bool IsStringASCII(const std::wstring& string)
{
    return DoIsStringASCII(string.data(), string.length());
}

bool IsUnicodeWhitespace(wchar_t c)
{
    // kWhitespaceWide is a NULL-terminated string
    for (const wchar_t* cur = kWhitespaceWide; *cur; ++cur)
    {
        if (*cur == c)
            return true;
    }
    return false;
}

template<typename STR>
STR CollapseWhitespaceT(const STR& text,
                        bool trim_sequences_with_line_breaks)
{
    STR result;
    result.resize(text.size());

    // Set flags to pretend we're already in a trimmed whitespace sequence, so we
    // will trim any leading whitespace.
    bool in_whitespace = true;
    bool already_trimmed = true;

    int chars_written = 0;
    for (typename STR::const_iterator i(text.begin()); i != text.end(); ++i)
    {
        if (IsUnicodeWhitespace(*i))
        {
            if (!in_whitespace)
            {
                // Reduce all whitespace sequences to a single space.
                in_whitespace = true;
                result[chars_written++] = L' ';
            }
            if (trim_sequences_with_line_breaks && !already_trimmed &&
                ((*i == '\n') || (*i == '\r')))
            {
                // Whitespace sequences containing CR or LF are eliminated entirely.
                already_trimmed = true;
                --chars_written;
            }
        }
        else
        {
            // Non-whitespace chracters are copied straight across.
            in_whitespace = false;
            already_trimmed = false;
            result[chars_written++] = *i;
        }
    }

    if (in_whitespace && !already_trimmed)
    {
        // Any trailing whitespace is eliminated.
        --chars_written;
    }

    result.resize(chars_written);
    return result;
}

std::wstring CollapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks)
{
    return CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

std::string CollapseWhitespaceASCII(const std::string& text,
                                    bool trim_sequences_with_line_breaks)
{
    return CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

int CompareCaseInsensitive(const std::string& first, const std::string& second)
{
    return _stricmp(first.c_str(), second.c_str());
}

int CompareCaseInsensitive(const std::wstring& first, const std::wstring& second)
{
    return _wcsicmp(first.c_str(), second.c_str());
}

template <class Str>
TrimPositions TrimStringT(const Str& input,
                          const typename Str::value_type* trim_chars,
                          TrimPositions positions,
                          Str& output)
{
    // Find the edges of leading/trailing whitespace as desired. Need to use
    // a StringPiece version of input to be able to call find* on it with the
    // StringPiece version of trim_chars (normally the trim_chars will be a
    // constant so avoid making a copy).
    Str input_piece(input);
    const size_t last_char = input.length() - 1;
    const size_t first_good_char = (positions & TRIM_LEADING) ?
        input_piece.find_first_not_of(trim_chars) : 0;
    const size_t last_good_char = (positions & TRIM_TRAILING) ?
        input_piece.find_last_not_of(trim_chars) : last_char;

    // When the string was all trimmed, report that we stripped off characters
    // from whichever position the caller was interested in. For empty input, we
    // stripped no characters, but we still need to clear |output|.
    if (input.empty() ||
        (first_good_char == Str::npos) || (last_good_char == Str::npos))
    {
        bool input_was_empty = input.empty();  // in case output == &input
        output.clear();
        return input_was_empty ? TRIM_NONE : positions;
    }

    // Trim.
    output = input.substr(first_good_char, last_good_char - first_good_char + 1);

    // Return where we trimmed from.
    return static_cast<TrimPositions>(
        ((first_good_char == 0) ? TRIM_NONE : TRIM_LEADING) |
        ((last_good_char == last_char) ? TRIM_NONE : TRIM_TRAILING));
}

bool TrimString(const std::string& input, std::string_view trim_chars, std::string& output)
{
    return TrimStringT(input, trim_chars.data(), TRIM_ALL, output) != TRIM_NONE;
}

bool TrimString(const std::wstring& input, std::wstring_view trim_chars, std::wstring& output)
{
    return TrimStringT(input, trim_chars.data(), TRIM_ALL, output) != TRIM_NONE;
}

TrimPositions TrimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring& output)
{
    return TrimStringT(input, kWhitespaceWide, positions, output);
}

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string& output)
{
    return TrimStringT(input, kWhitespaceASCII, positions, output);
}

std::wstring ToUpper(const std::wstring& in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towupper);
    return out;
}

std::wstring ToLower(const std::wstring& in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towlower);
    return out;
}

std::string ToUpperASCII(const std::string& in)
{
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string ToLowerASCII(const std::string& in)
{
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

const std::string& EmptyString()
{
    return kEmptyString;
}

const std::wstring& EmptyStringW()
{
    return kEmptyStringWide;
}

} // namespace aspia
