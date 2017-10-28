//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/string_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_util.h"
#include "base/strings/string_util_constants.h"
#include "base/logging.h"

#include <algorithm>
#include <bitset>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <strsafe.h>

namespace aspia {

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

bool StringIsUtf8(const char* data, size_t length)
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

std::string StringPrintf(const char* format, ...)
{
    va_list args;

    va_start(args, format);

    int len = _vscprintf(format, args);
    CHECK(len >= 0) << errno;

    std::string out;
    out.resize(len);

    CHECK(SUCCEEDED(StringCchVPrintfA(&out[0], len + 1, format, args)));

    va_end(args);

    return out;
}

std::wstring StringPrintfW(const WCHAR* format, ...)
{
    va_list args;

    va_start(args, format);

    int len = _vscwprintf(format, args);
    CHECK(len >= 0) << errno;

    std::wstring out;
    out.resize(len);

    CHECK(SUCCEEDED(StringCchVPrintfW(&out[0], len + 1, format, args)));

    va_end(args);

    return out;
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

void ToUpper(std::wstring& string)
{
    std::transform(string.begin(), string.end(), string.begin(), std::towupper);
}

std::wstring ToUpperCopy(const std::wstring& in)
{
    std::wstring out(in);
    ToUpper(out);
    return out;
}

void ToLower(std::wstring& string)
{
    std::transform(string.begin(), string.end(), string.begin(), std::towlower);
}

std::wstring ToLowerCopy(const std::wstring& in)
{
    std::wstring out(in);
    ToLower(out);
    return out;
}

std::string ToHexString(const uint8_t* data, size_t data_size)
{
    DCHECK(data);

    std::stringstream ss;

    for (size_t i = 0; i < data_size; ++i)
    {
        ss << std::setw(2) << std::setfill('0') << std::hex
           << std::uppercase << static_cast<int>(data[i]);
    }

    return ss.str();
}

std::string ToHexString(uint64_t data)
{
    return ToHexString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToHexString(uint32_t data)
{
    return ToHexString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToHexString(uint16_t data)
{
    return ToHexString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToHexString(uint8_t data)
{
    return ToHexString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToBinaryString(const uint8_t* data, size_t data_size)
{
    DCHECK(data);

    std::string string;

    for (size_t i = 0; i < data_size; ++i)
    {
        std::bitset<8> c(data[i]);
        string += c.to_string();
    }

    return string;
}

std::string ToBinaryString(uint64_t data)
{
    return ToBinaryString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToBinaryString(uint32_t data)
{
    return ToBinaryString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToBinaryString(uint16_t data)
{
    return ToBinaryString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

std::string ToBinaryString(uint8_t data)
{
    return ToBinaryString(reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

} // namespace aspia
