//
// SmartCafe Project
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

#include "base/mac/nsstring_conversions.h"

#include <vector>

namespace base {

namespace  {

//--------------------------------------------------------------------------------------------------
// Convert the supplied CFString into the specified encoding, and return it as
// an STL string of the template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring, CFStringEncoding encoding)
{
    CFIndex length = CFStringGetLength(cfstring);
    if (length == 0)
      return StringType();

    CFRange whole_string = CFRangeMake(0, length);
    CFIndex out_size;
    CFIndex converted = CFStringGetBytes(cfstring,
                                         whole_string,
                                         encoding,
                                         0,      // lossByte
                                         false,  // isExternalRepresentation
                                         NULL,   // buffer
                                         0,      // maxBufLen
                                         &out_size);
    if (converted == 0 || out_size == 0)
        return StringType();

    // out_size is the number of UInt8-sized units needed in the destination.
    // A buffer allocated as UInt8 units might not be properly aligned to
    // contain elements of StringType::value_type.  Use a container for the
    // proper value_type, and convert out_size by figuring the number of
    // value_type elements per UInt8.  Leave room for a NUL terminator.
    typename StringType::size_type elements =
        out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

    std::vector<typename StringType::value_type> out_buffer(elements);
    converted = CFStringGetBytes(cfstring,
                                 whole_string,
                                 encoding,
                                 0,      // lossByte
                                 false,  // isExternalRepresentation
                                 reinterpret_cast<UInt8*>(&out_buffer[0]),
                                 out_size,
                                 NULL);  // usedBufLen
    if (converted == 0)
        return StringType();

    out_buffer[elements - 1] = '\0';
    return StringType(out_buffer.data(), elements - 1);
}

//--------------------------------------------------------------------------------------------------
// Given a StringPiece |in| with an encoding specified by |in_encoding|, return
// it as a CFStringRef. Returns NULL on failure.
template <typename CharType>
static CFStringRef StringPieceToCFStringWithEncodingsT(
    std::basic_string_view<CharType> in,
    CFStringEncoding in_encoding)
{
    const auto in_length = in.length();
    if (in_length == 0)
        return CFSTR("");

    return CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
        in_length * sizeof(typename std::basic_string_view<CharType>::value_type),
        in_encoding, false);
}

// Specify the byte ordering explicitly, otherwise CFString will be confused
// when strings don't carry BOMs, as they typically won't.
static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;
#ifdef __BIG_ENDIAN__
static const CFStringEncoding kMediumStringEncoding = kCFStringEncodingUTF16BE;
#elif defined(__LITTLE_ENDIAN__)
static const CFStringEncoding kMediumStringEncoding = kCFStringEncodingUTF16LE;
#endif  // __LITTLE_ENDIAN__

} // namespace

//--------------------------------------------------------------------------------------------------
CFStringRef utf8ToCFStringRef(std::string_view utf8)
{
    return StringPieceToCFStringWithEncodingsT<char>(utf8, kNarrowStringEncoding);
}

//--------------------------------------------------------------------------------------------------
CFStringRef utf16ToCFStringRef(std::u16string_view utf16)
{
    return StringPieceToCFStringWithEncodingsT<char16_t>(utf16, kMediumStringEncoding);
}

//--------------------------------------------------------------------------------------------------
NSString* utf8ToNSString(std::string_view utf8)
{
    return [reinterpret_cast<NSString*>(utf8ToCFStringRef(utf8)) autorelease];
}

//--------------------------------------------------------------------------------------------------
NSString* utf16ToNSString(std::u16string_view utf16)
{
    return [reinterpret_cast<NSString*>(utf16ToCFStringRef(utf16)) autorelease];
}

//--------------------------------------------------------------------------------------------------
std::string CFStringRefToUtf8(CFStringRef ref)
{
    return CFStringToSTLStringWithEncodingT<std::string>(ref, kNarrowStringEncoding);
}

//--------------------------------------------------------------------------------------------------
std::u16string CFStringRefToUtf16(CFStringRef ref)
{
    return CFStringToSTLStringWithEncodingT<std::u16string>(ref, kMediumStringEncoding);
}

//--------------------------------------------------------------------------------------------------
std::string NSStringToUtf8(NSString* nsstring)
{
    if (!nsstring)
        return std::string();
    return CFStringRefToUtf8(reinterpret_cast<CFStringRef>(nsstring));
}

//--------------------------------------------------------------------------------------------------
std::u16string NSStringToUtf16(NSString* nsstring)
{
    if (!nsstring)
        return std::u16string();
    return CFStringRefToUtf16(reinterpret_cast<CFStringRef>(nsstring));
}

} // namespace base
