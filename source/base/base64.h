//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_BASE64_H
#define BASE_BASE64_H

#include "base/macros_magic.h"

#include <string>

namespace base {

class Base64
{
public:
    // Encodes the input string in base64. The encoding can be done in-place.
    static void encode(std::string_view input, std::string* output);

    // Encodes the input string in base64.
    static std::string encode(std::string_view input);

    // Decodes the base64 input string. Returns true if successful and false otherwise.
    // The output string is only modified if successful. The decoding can be done in-place.
    static bool decode(std::string_view input, std::string* output);

    // Decodes the base64 input string. If an error occurred during decoding, an empty string
    // is returned.
    static std::string decode(std::string_view input);

    template <class InputBufferT, class OutputBufferT>
    static OutputBufferT encodeT(const InputBufferT& input)
    {
        const size_t input_length = input.size();
        if (!input_length)
            return OutputBufferT();

        OutputBufferT result;
        result.resize(encodedLength(input_length));

        const size_t output_size =
            encodeImpl(reinterpret_cast<char*>(result.data()),
                       reinterpret_cast<const char*>(input.data()),
                       input_length);
        result.resize(output_size);

        return result;
    }

    static const size_t kErrorResult = static_cast<size_t>(-1);

    template <class InputBufferT, class OutputBufferT>
    static bool decodeT(const InputBufferT& input, OutputBufferT* output)
    {
        const size_t input_length = input.length();
        if (!input_length)
            return false;

        OutputBufferT temp;
        temp.resize(decodedLength(input_length));

        const size_t output_size =
            decodeImpl(reinterpret_cast<char*>(temp.data()),
                       reinterpret_cast<const char*>(input.data()),
                       input_length);
        if (output_size == kErrorResult)
            return false;

        temp.resize(output_size);
        output->swap(temp);
        return true;
    }

    template <class InputBufferT, class OutputBufferT>
    static OutputBufferT decodeT(const InputBufferT& input)
    {
        OutputBufferT result;

        if (!decodeT(input, &result))
            return OutputBufferT();

        return result;
    }

private:
    static size_t encodeImpl(char* dest, const char* str, size_t len);
    static size_t decodeImpl(char* dest, const char* src, size_t len);

    static size_t encodedLength(size_t len);
    static size_t decodedLength(size_t len);

    DISALLOW_COPY_AND_ASSIGN(Base64);
};

} // namespace base

#endif // BASE_BASE64_H
