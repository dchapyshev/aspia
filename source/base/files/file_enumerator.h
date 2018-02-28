//
// PROJECT:         Aspia
// FILE:            base/files/file_enumerator.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE_FILES_FILE_ENUMERATOR_H
#define _ASPIA_BASE_FILES_FILE_ENUMERATOR_H

#include <stddef.h>
#include <stdint.h>

#include <experimental/filesystem>
#include <stack>
#include <vector>

#include "base/macros.h"

namespace aspia {

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// This is blocking. Do not use on critical threads.
//
// Example:
//
//   base::FileEnumerator enum(my_dir, false, base::FileEnumerator::FILES,
//                             FILE_PATH_LITERAL("*.txt"));
//   for (base::FilePath name = enum.Next(); !name.empty(); name = enum.Next())
//     ...
class FileEnumerator
{
public:
    // Note: copy & assign supported.
    class FileInfo
    {
    public:
        FileInfo();
        ~FileInfo() = default;

        bool IsDirectory() const;

        // The name of the file. This will not include any path information. This
        // is in constrast to the value returned by FileEnumerator.Next() which
        // includes the |root_path| passed into the FileEnumerator constructor.
        std::experimental::filesystem::path GetName() const;

        int64_t GetSize() const;
        time_t GetLastModifiedTime() const;

        // Note that the cAlternateFileName (used to hold the "short" 8.3 name)
        // of the WIN32_FIND_DATA will be empty. Since we don't use short file
        // names, we tell Windows to omit it which speeds up the query slightly.
        const WIN32_FIND_DATA& find_data() const { return find_data_; }

    private:
        friend class FileEnumerator;

        WIN32_FIND_DATA find_data_;
    };

    enum FileType
    {
        FILES           = 1 << 0,
        DIRECTORIES     = 1 << 1,
        INCLUDE_DOT_DOT = 1 << 2
    };

    // Search policy for intermediate folders.
    enum class FolderSearchPolicy
    {
        // Recursive search will pass through folders whose names match the
        // pattern. Inside each one, all files will be returned. Folders with names
        // that do not match the pattern will be ignored within their interior.
        MATCH_ONLY,
        // Recursive search will pass through every folder and perform pattern
        // matching inside each one.
        ALL,
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
    FileEnumerator(const std::experimental::filesystem::path& root_path,
                   bool recursive,
                   int file_type);
    FileEnumerator(const std::experimental::filesystem::path& root_path,
                   bool recursive,
                   int file_type,
                   const std::experimental::filesystem::path::string_type& pattern);
    FileEnumerator(const std::experimental::filesystem::path& root_path,
                   bool recursive,
                   int file_type,
                   const std::experimental::filesystem::path::string_type& pattern,
                   FolderSearchPolicy folder_search_policy);
    ~FileEnumerator();

    // Returns the next file or an empty string if there are no more results.
    //
    // The returned path will incorporate the |root_path| passed in the
    // constructor: "<root_path>/file_name.txt". If the |root_path| is absolute,
    // then so will be the result of Next().
    std::experimental::filesystem::path Next();

    // Write the file info into |info|.
    FileInfo GetInfo() const;

private:
    // Returns true if the given path should be skipped in enumeration.
    bool ShouldSkip(const std::experimental::filesystem::path& path) const;

    bool IsTypeMatched(bool is_dir) const;

    bool IsPatternMatched(const std::experimental::filesystem::path& src) const;

    // True when find_data_ is valid.
    bool has_find_data_ = false;
    WIN32_FIND_DATA find_data_;
    HANDLE find_handle_ = INVALID_HANDLE_VALUE;

    std::experimental::filesystem::path root_path_;
    const bool recursive_;
    const int file_type_;
    std::experimental::filesystem::path::string_type pattern_;
    const FolderSearchPolicy folder_search_policy_;

    // A stack that keeps track of which subdirectories we still need to
    // enumerate in the breadth-first search.
    std::stack<std::experimental::filesystem::path> pending_paths_;

    DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

}  // namespace aspia

#endif // _ASPIA_BASE_FILES_FILE_ENUMERATOR_H
