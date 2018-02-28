//
// PROJECT:         Aspia
// FILE:            base/files/file_helpers.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_helpers.h"

namespace aspia {

namespace {

// Minimal path name is 2 characters (for example: "C:").
constexpr size_t kMinPathLength = 2;

// Under Window the length of a full path is 260 characters.
constexpr size_t kMaxPathLength = MAX_PATH;

// The file name can not be shorter than 1 character.
constexpr size_t kMinNameLength = 1;

// For FAT: file and folder names may be up to 255 characters long.
// For NTFS: file and folder names may be up to 256 characters long.
// We use FAT variant: 255 characters long.
constexpr size_t kMaxNameLength = (MAX_PATH - 5);

bool IsValidNameChar(wchar_t character)
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

bool IsValidPathChar(wchar_t character)
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

} // namespace

bool IsValidFileName(const std::experimental::filesystem::path& path)
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

bool IsValidPathName(const std::experimental::filesystem::path& path)
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
