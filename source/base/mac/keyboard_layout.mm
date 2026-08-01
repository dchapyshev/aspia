//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/mac/keyboard_layout.h"

// Keep Carbon's AssertMacros.h from defining the bare check()/verify()/require() macros that clash
// with other code.
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"

//--------------------------------------------------------------------------------------------------
static QString inputSourceId(TISInputSourceRef source)
{
    CFStringRef id = static_cast<CFStringRef>(
        TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
    return id ? QString::fromCFString(id) : QString();
}

//--------------------------------------------------------------------------------------------------
QList<KeyboardLayout> enabledKeyboardLayouts()
{
    QList<KeyboardLayout> result;

    // Keyboard layouts only (not input methods or palettes), enabled and selectable.
    const void* keys[] = { kTISPropertyInputSourceType, kTISPropertyInputSourceIsSelectCapable };
    const void* values[] = { kTISTypeKeyboardLayout, kCFBooleanTrue };
    ScopedCFTypeRef<CFDictionaryRef> filter(CFDictionaryCreate(nullptr, keys, values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!filter)
    {
        LOG(ERROR) << "Unable to create the input source filter";
        return result;
    }

    ScopedCFTypeRef<CFArrayRef> sources(TISCreateInputSourceList(filter.get(), false));
    if (!sources)
        return result;

    CFIndex count = CFArrayGetCount(sources.get());
    for (CFIndex i = 0; i < count; ++i)
    {
        TISInputSourceRef source = static_cast<TISInputSourceRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(sources.get(), i)));

        KeyboardLayout layout;
        layout.id = inputSourceId(source);

        CFStringRef name = static_cast<CFStringRef>(
            TISGetInputSourceProperty(source, kTISPropertyLocalizedName));
        if (name)
            layout.name = QString::fromCFString(name);

        if (!layout.id.isEmpty())
            result.append(layout);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString currentKeyboardLayout()
{
    ScopedCFTypeRef<TISInputSourceRef> source(TISCopyCurrentKeyboardInputSource());
    if (!source)
        return QString();

    return inputSourceId(source.get());
}

//--------------------------------------------------------------------------------------------------
void selectKeyboardLayout(const QString& id)
{
    ScopedCFTypeRef<CFStringRef> wanted(id.toCFString());

    const void* keys[] = { kTISPropertyInputSourceID };
    const void* values[] = { wanted.get() };
    ScopedCFTypeRef<CFDictionaryRef> filter(CFDictionaryCreate(nullptr, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    if (!filter)
    {
        LOG(ERROR) << "Unable to create the input source filter";
        return;
    }

    ScopedCFTypeRef<CFArrayRef> sources(TISCreateInputSourceList(filter.get(), false));
    if (!sources)
        return;

    if (CFArrayGetCount(sources.get()) > 0)
    {
        TISInputSourceRef source = static_cast<TISInputSourceRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(sources.get(), 0)));
        TISSelectInputSource(source);
    }
}
