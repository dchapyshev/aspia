//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/module.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/module.h"
#include "base/logging.h"

namespace aspia {

UiModule::UiModule(HINSTANCE instance) :
    instance_(instance)
{
    // Nothing
}

UiModule::UiModule(const UiModule& other)
{
    instance_ = other.instance_;
}

// static
UiModule UiModule::Current()
{
    return UiModule(GetModuleHandleW(nullptr));
}

UiModule& UiModule::operator=(const UiModule& other)
{
    instance_ = other.instance_;
    return *this;
}

HINSTANCE UiModule::Handle() const
{
    return instance_;
}

static int GetLengthOfStringResource(HINSTANCE instance, UINT string_id)
{
    HRSRC source = FindResourceW(instance,
                                 reinterpret_cast<LPCWSTR>(MAKEINTRESOURCEW((string_id >> 4) + 1)),
                                 reinterpret_cast<LPCWSTR>(RT_STRING));
    if (!source)
        return -1;

    HGLOBAL res = LoadResource(instance, source);
    if (res)
    {
        WCHAR* str = reinterpret_cast<WCHAR*>(LockResource(res));
        if (str)
        {
            // Find the string we're looking for
            string_id &= 0xF; // position in the block, same as % 16

            for (UINT x = 0; x < string_id; ++x)
            {
                str += (*str) + 1;
            }

            // Found the string
            return *str;
        }
    }

    return -1;
}

std::wstring UiModule::string(UINT resource_id) const
{
    int length = GetLengthOfStringResource(instance_, resource_id);

    CHECK_GE(length, 0) << "String resource not found: " << resource_id;

    std::wstring string;
    string.resize(length);

    CHECK_EQ(LoadStringW(instance_, resource_id, &string[0], length + 1), length);

    return string;
}

HICON UiModule::icon(UINT resource_id, int width, int height, UINT flags) const
{
    return reinterpret_cast<HICON>(LoadImageW(instance_,
                                              MAKEINTRESOURCEW(resource_id),
                                              IMAGE_ICON,
                                              width,
                                              height,
                                              flags));
}

HMENU UiModule::menu(UINT resource_id) const
{
    return LoadMenuW(instance_, MAKEINTRESOURCEW(resource_id));
}

} // namespace aspia
