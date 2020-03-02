//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
// This file based on MODP_B64 - High performance base64 encoder/decoder (BSD License).
// Copyright (C) 2005, 2006  Nick Galbreath - nickg [at] modp [dot] com
//

#include "base/base64.h"

#include "base/logging.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

// static
void Base64::encode(const std::string& input, std::string* output)
{
    DCHECK(output);
    output->swap(encode(input));
}

// static
std::string Base64::encode(const std::string& input)
{
    return encodeT<std::string, std::string>(input);
}

// static
bool Base64::decode(const std::string& input, std::string* output)
{
    DCHECK(output);
    return decodeT<std::string>(input, output);
}

// static
std::string Base64::decode(const std::string& input)
{
    return decodeT<std::string, std::string>(input);
}

// static
size_t Base64::encodeImpl(char* dest, const char* str, size_t len)
{
    return modp_b64_encode(dest, str, len);
}

// static
size_t Base64::decodeImpl(char* dest, const char* src, size_t len)
{
    return modp_b64_decode(dest, src, len);
}

// static
size_t Base64::encodedLength(size_t len)
{
    return modp_b64_encode_strlen(len) + 1;
}

// static
size_t Base64::decodedLength(size_t len)
{
    return modp_b64_decode_len(len);
}

} // namespace base
