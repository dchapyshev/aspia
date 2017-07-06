//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/file_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_helpers.h"

namespace aspia {

// Minimal path name is 2 characters (for example: "C:").
static const size_t kMinPathLength = 2;

// Under Window the length of a full path is 260 characters.
static const size_t kMaxPathLength = MAX_PATH;

// The file name can not be shorter than 1 character.
static const size_t kMinNameLength = 1;

// For FAT: file and folder names may be up to 255 characters long.
// For NTFS: file and folder names may be up to 256 characters long.
// We use FAT variant: 255 characters long.
static const size_t kMaxNameLength = (MAX_PATH - 5);

static bool IsValidNameChar(wchar_t character)
{
    switch (character)
    {
        // Invalid characters for file name.
        case L'/':
        case L'\\':
        case L':':
        case L'*':
        case L'?':
        case L'"':
        case L'<':
        case L'>':
        case L'|':
            return false;

        default:
            return true;
    }
}

bool IsValidFileName(const FilePath& path)
{
    std::wstring native_path(path.wstring());

    size_t length = native_path.length();

    if (length < kMinNameLength || length > kMaxNameLength)
        return false;

    for (const auto& character : native_path)
    {
        if (!IsValidNameChar(character))
            return false;
    }

    return true;
}

static bool IsValidPathChar(wchar_t character)
{
    switch (character)
    {
        // Invalid characters for path name.
        case L'*':
        case L'?':
        case L'"':
        case L'<':
        case L'>':
        case L'|':
            return false;

        default:
            return true;
    }
}

bool IsValidPathName(const FilePath& path)
{
    std::wstring native_path(path.wstring());

    size_t length = native_path.length();

    if (length < kMinPathLength || length > kMaxPathLength)
        return false;

    for (const auto& character : native_path)
    {
        if (!IsValidPathChar(character))
            return false;
    }

    return true;
}

} // namespace aspia
