//
// PROJECT:         Aspia
// FILE:            base/strings/string_util.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_util.h"

#include <algorithm>
#include <cwctype>
#include <cctype>
#include <strsafe.h>

#include "base/strings/string_util_constants.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

namespace {

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

std::wstring ToUpper(std::wstring_view in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towupper);
    return out;
}

std::wstring ToLower(std::wstring_view in)
{
    std::wstring out(in);
    std::transform(out.begin(), out.end(), out.begin(), std::towlower);
    return out;
}

} // namespace aspia
