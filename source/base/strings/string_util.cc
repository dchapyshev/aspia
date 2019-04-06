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

#include "base/strings/string_util.h"
#include "base/strings/string_util_constants.h"
#include "base/strings/unicode.h"

#include <algorithm>
#include <cwctype>
#include <cctype>

namespace base {

namespace {

const std::string kEmptyString;
const std::wstring kEmptyStringWide;

// Assuming that a pointer is the size of a "machine word", then
// uintptr_t is an integer type that is also a machine word.
using MachineWord = uintptr_t;

bool isMachineWordAligned(const void* pointer)
{
    return !(reinterpret_cast<MachineWord>(pointer) & (sizeof(MachineWord) - 1));
}

template <typename CharacterType>
struct NonASCIIMask;

template <>
struct NonASCIIMask<char>
{
    static constexpr MachineWord value()
    {
        return static_cast<MachineWord>(0x8080808080808080ULL);
    }
};

template <>
struct NonASCIIMask<char16_t>
{
    static constexpr MachineWord value()
    {
        return static_cast<MachineWord>(0xFF80FF80FF80FF80ULL);
    }
};

template <>
struct NonASCIIMask<wchar_t>
{
    static constexpr MachineWord value()
    {
        return static_cast<MachineWord>(0xFFFFFF80FFFFFF80ULL);
    }
};

template <class Char>
bool doIsStringASCII(const Char* characters, size_t length)
{
    if (!length)
        return true;

    constexpr MachineWord non_ascii_bit_mask = NonASCIIMask<Char>::value();
    MachineWord all_char_bits = 0;
    const Char* end = characters + length;

    // Prologue: align the input.
    while (!isMachineWordAligned(characters) && characters < end)
        all_char_bits |= *characters++;
    if (all_char_bits & non_ascii_bit_mask)
        return false;

    // Compare the values of CPU word size.
    constexpr size_t chars_per_word = sizeof(MachineWord) / sizeof(Char);
    constexpr int batch_count = 16;
    while (characters <= end - batch_count * chars_per_word)
    {
        all_char_bits = 0;
        for (int i = 0; i < batch_count; ++i)
        {
            all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
            characters += chars_per_word;
        }
        if (all_char_bits & non_ascii_bit_mask)
            return false;
    }

    // Process the remaining words.
    all_char_bits = 0;
    while (characters <= end - chars_per_word)
    {
        all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
        characters += chars_per_word;
    }

    // Process the remaining bytes.
    while (characters < end)
        all_char_bits |= *characters++;

    return !(all_char_bits & non_ascii_bit_mask);
}

template<typename STR>
STR collapseWhitespaceT(const STR& text, bool trim_sequences_with_line_breaks)
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
        if (isUnicodeWhitespace(*i))
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

template<typename StringType>
void removeCharsT(StringType* str, std::basic_string_view<typename StringType::value_type> substr)
{
    size_t pos;

    while ((pos = str->find(substr)) != StringType::npos)
        str->erase(pos, substr.length());
}

} // namespace

std::string replaceLfByCrLf(const std::string& in)
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
            *out_p++ = '\r';

        *out_p++ = c;
    }

    out.resize(out_p - out_p_begin);
    return out;
}

std::string replaceCrLfByLf(const std::string& in)
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

bool isStringUTF8(const char* data, size_t length)
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

bool isStringUTF8(const std::string& string)
{
    return isStringUTF8(string.data(), string.length());
}

bool isStringASCII(const char* data, size_t length)
{
    return doIsStringASCII(data, length);
}

bool isStringASCII(const std::string& string)
{
    return doIsStringASCII(string.data(), string.length());
}

bool isStringASCII(const wchar_t* data, size_t length)
{
    return doIsStringASCII(data, length);
}

bool isStringASCII(const std::wstring& string)
{
    return doIsStringASCII(string.data(), string.length());
}

bool isUnicodeWhitespace(wchar_t c)
{
    // kWhitespaceWide is a NULL-terminated string
    for (const wchar_t* cur = kWhitespaceWide; *cur; ++cur)
    {
        if (*cur == c)
            return true;
    }
    return false;
}

std::wstring collapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks)
{
    return collapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

std::string collapseWhitespaceASCII(const std::string& text,
                                    bool trim_sequences_with_line_breaks)
{
    return collapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

int compareCaseInsensitiveASCII(const std::string& first, const std::string& second)
{
    return _stricmp(first.c_str(), second.c_str());
}

int compareCaseInsensitive(const std::wstring& first, const std::wstring& second)
{
    return _wcsicmp(first.c_str(), second.c_str());
}

template<typename CharType>
bool startsWithT(std::basic_string_view<CharType> str, std::basic_string_view<CharType> search_for)
{
    if (search_for.size() > str.size())
        return false;

    return str.substr(0, search_for.size()) == search_for;
}

bool startsWith(std::string_view str, std::string_view search_for)
{
    return startsWithT<char>(str, search_for);
}

bool startsWith(std::wstring_view str, std::wstring_view search_for)
{
    return startsWithT<wchar_t>(str, search_for);
}

template <typename CharType>
bool endsWithT(std::basic_string_view<CharType> str, std::basic_string_view<CharType> search_for)
{
    if (search_for.size() > str.size())
        return false;

    std::basic_string_view<CharType> source =
        str.substr(str.size() - search_for.size(), search_for.size());

    return source == search_for;
}

bool endsWith(std::string_view str, std::string_view search_for)
{
    return endsWithT<char>(str, search_for);
}

bool endsWith(std::wstring_view str, std::wstring_view search_for)
{
    return endsWithT<wchar_t>(str, search_for);
}

template <class Str>
TrimPositions trimStringT(const Str& input,
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

bool trimString(const std::string& input, std::string_view trim_chars, std::string& output)
{
    return trimStringT(input, trim_chars.data(), TRIM_ALL, output) != TRIM_NONE;
}

bool trimString(const std::wstring& input, std::wstring_view trim_chars, std::wstring& output)
{
    return trimStringT(input, trim_chars.data(), TRIM_ALL, output) != TRIM_NONE;
}

TrimPositions trimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring& output)
{
    return trimStringT(input, kWhitespaceWide, positions, output);
}

TrimPositions trimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string& output)
{
    return trimStringT(input, kWhitespaceASCII, positions, output);
}

void removeChars(std::string* str, std::string_view substr)
{
    removeCharsT(str, substr);
}

void removeChars(std::wstring* str, std::wstring_view substr)
{
    removeCharsT(str, substr);
}

std::wstring toUpper(std::wstring_view in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towupper);
    return out;
}

std::wstring toLower(std::wstring_view in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towlower);
    return out;
}

std::string toUpperASCII(std::string_view in)
{
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string toLowerASCII(std::string_view in)
{
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

const std::string& emptyString()
{
    return kEmptyString;
}

const std::wstring& emptyStringW()
{
    return kEmptyStringWide;
}

} // namespace base
