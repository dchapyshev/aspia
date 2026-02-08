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

#ifndef BASE_MAC_NSSTRING_CONVERSIONS_H
#define BASE_MAC_NSSTRING_CONVERSIONS_H

#include <string>

#include <CoreFoundation/CoreFoundation.h>
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif

namespace base {

CFStringRef utf8ToCFStringRef(std::string_view utf8);
CFStringRef utf16ToCFStringRef(std::u16string_view utf16);

NSString* utf8ToNSString(std::string_view utf8);
NSString* utf16ToNSString(std::u16string_view utf16);

std::string CFStringRefToUtf8(CFStringRef ref);
std::u16string CFStringRefToUtf16(CFStringRef ref);

std::string NSStringToUtf8(NSString* nsstring);
std::u16string NSStringToUtf16(NSString* nsstring);

} // namespace base

#endif // BASE_MAC_NSSTRING_CONVERSIONS_H
