//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/file_enumerator.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILE_ENUMERATOR_H
#define _ASPIA_BASE__FILE_ENUMERATOR_H

#include "base/macros.h"

#include <string>
#include <stack>
#include <vector>

namespace aspia {

class FileEnumerator
{
public:
    class FileInfo
    {
    public:
        FileInfo();
        ~FileInfo() = default;

        bool IsDirectory() const;

        // The name of the file. This will not include any path information. This
        // is in constrast to the value returned by FileEnumerator.Next() which
        // includes the |root_path| passed into the FileEnumerator constructor.
        std::wstring GetName() const;

        int64_t GetSize() const;

    private:
        friend class FileEnumerator;

        WIN32_FIND_DATA find_data_;
    };

    enum FileType
    {
        FILES           = 1 << 0,
        DIRECTORIES     = 1 << 1,
        INCLUDE_DOT_DOT = 1 << 1
    };

    // |root_path| is the starting directory to search for. It may or may not end
    // in a slash.
    //
    // If |recursive| is true, this will enumerate all matches in any
    // subdirectories matched as well. It does a breadth-first search, so all
    // files in one directory will be returned before any files in a
    // subdirectory.
    //
    // |file_type|, a bit mask of FileType, specifies whether the enumerator
    // should match files, directories, or both.
    //
    // |pattern| is an optional pattern for which files to match. This
    // works like shell globbing. For example, "*.txt" or "Foo???.doc".
    // However, be careful in specifying patterns that aren't cross platform
    // since the underlying code uses OS-specific matching routines.  In general,
    // Windows matching is less featureful than others, so test there first.
    // If unspecified, this will match all files.
    // NOTE: the pattern only matches the contents of root_path, not files in
    // recursive subdirectories.
    // TODO(erikkay): Fix the pattern matching to work at all levels.
    FileEnumerator(const std::wstring& root_path,
                   bool resursive,
                   int file_type);

    FileEnumerator(const std::wstring& root_path,
                   bool resursive,
                   int file_type,
                   const std::wstring& pattern);

    ~FileEnumerator();

    // Returns the next file or an empty string if there are no more results.
    std::wstring Next();

    // Write the file info into |info|.
    FileInfo GetInfo() const;

private:
    // Returns true if the given path should be skipped in enumeration.
    bool ShouldSkip(const std::wstring& path);

    // True when find_data_ is valid.
    bool has_find_data_ = false;
    WIN32_FIND_DATA find_data_;
    HANDLE find_handle_ = INVALID_HANDLE_VALUE;

    std::wstring root_path_;
    bool recursive_;
    int file_type_;
    std::wstring pattern_; // Empty when we want to find everything.

    // A stack that keeps track of which subdirectories we still need to
    // enumerate in the breadth-first search.
    std::stack<std::wstring> pending_paths_;

    DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__FILE_ENUMERATOR_H
