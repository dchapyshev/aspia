//
// PROJECT:         Aspia
// FILE:            base/files/file_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_enumerator.h"

#include <shlwapi.h>
#include <stdint.h>
#include <string.h>

#include "base/files/file_util.h"
#include "base/logging.h"

namespace aspia {

namespace {

std::experimental::filesystem::path BuildSearchFilter(
    FileEnumerator::FolderSearchPolicy policy,
    const std::experimental::filesystem::path& root_path,
    const std::experimental::filesystem::path::string_type& pattern)
{
    std::experimental::filesystem::path path(root_path);

    // MATCH_ONLY policy filters incoming files by pattern on OS side. ALL policy
    // collects all files and filters them manually.
    switch (policy)
    {
        case FileEnumerator::FolderSearchPolicy::MATCH_ONLY:
            path.append(pattern);
            break;

        case FileEnumerator::FolderSearchPolicy::ALL:
            path.append(L"*");
            break;

        default:
            DLOG(FATAL) << "Unknown search policy: " << static_cast<int>(policy);
            return {};
    }

    return path;
}

} // namespace

// FileEnumerator::FileInfo ----------------------------------------------------

FileEnumerator::FileInfo::FileInfo()
{
    memset(&find_data_, 0, sizeof(find_data_));
}

bool FileEnumerator::FileInfo::IsDirectory() const
{
    return (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::experimental::filesystem::path FileEnumerator::FileInfo::GetName() const
{
    return std::experimental::filesystem::path(find_data_.cFileName);
}

int64_t FileEnumerator::FileInfo::GetSize() const
{
    ULARGE_INTEGER size;
    size.HighPart = find_data_.nFileSizeHigh;
    size.LowPart = find_data_.nFileSizeLow;
    DCHECK_LE(size.QuadPart, static_cast<ULONGLONG>(std::numeric_limits<int64_t>::max()));
    return static_cast<int64_t>(size.QuadPart);
}

time_t FileEnumerator::FileInfo::GetLastModifiedTime() const
{
    return UnixTimeFromFileTime(find_data_.ftLastWriteTime);
}

// FileEnumerator --------------------------------------------------------------

FileEnumerator::FileEnumerator(const std::experimental::filesystem::path& root_path,
                               bool recursive,
                               int file_type)
    : FileEnumerator(root_path,
                     recursive,
                     file_type,
                     std::experimental::filesystem::path::string_type(),
                     FolderSearchPolicy::MATCH_ONLY)
{
    // Nothing
}

FileEnumerator::FileEnumerator(const std::experimental::filesystem::path& root_path,
                               bool recursive,
                               int file_type,
                               const std::experimental::filesystem::path::string_type& pattern)
    : FileEnumerator(root_path,
                     recursive,
                     file_type,
                     pattern,
                     FolderSearchPolicy::MATCH_ONLY)
{
    // Nothing
}

FileEnumerator::FileEnumerator(const std::experimental::filesystem::path& root_path,
                               bool recursive,
                               int file_type,
                               const std::experimental::filesystem::path::string_type& pattern,
                               FolderSearchPolicy folder_search_policy)
    : recursive_(recursive),
      file_type_(file_type),
      pattern_(!pattern.empty() ? pattern : L"*"),
      folder_search_policy_(folder_search_policy)
{
    // INCLUDE_DOT_DOT must not be specified if recursive.
    DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
    memset(&find_data_, 0, sizeof(find_data_));
    pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator()
{
    if (find_handle_ != INVALID_HANDLE_VALUE)
        FindClose(find_handle_);
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const
{
    if (!has_find_data_)
        return FileInfo();

    FileInfo ret;
    memcpy(&ret.find_data_, &find_data_, sizeof(find_data_));
    return ret;
}

std::experimental::filesystem::path FileEnumerator::Next()
{
    while (has_find_data_ || !pending_paths_.empty())
    {
        if (!has_find_data_)
        {
            // The last find FindFirstFile operation is done, prepare a new one.
            root_path_ = pending_paths_.top();
            pending_paths_.pop();

            // Start a new find operation.
            const std::experimental::filesystem::path src =
                BuildSearchFilter(folder_search_policy_, root_path_, pattern_);

            find_handle_ = FindFirstFileExW(src.c_str(),
                                            FindExInfoBasic,  // Omit short name.
                                            &find_data_, FindExSearchNameMatch,
                                            nullptr, FIND_FIRST_EX_LARGE_FETCH);
            has_find_data_ = true;
        }
        else
        {
            // Search for the next file/directory.
            if (!FindNextFileW(find_handle_, &find_data_))
            {
                FindClose(find_handle_);
                find_handle_ = INVALID_HANDLE_VALUE;
            }
        }

        if (INVALID_HANDLE_VALUE == find_handle_)
        {
            has_find_data_ = false;

            // MATCH_ONLY policy clears pattern for matched subfolders. ALL policy
            // applies pattern for all subfolders.
            if (folder_search_policy_ == FolderSearchPolicy::MATCH_ONLY)
            {
                // This is reached when we have finished a directory and are advancing
                // to the next one in the queue. We applied the pattern (if any) to the
                // files in the root search directory, but for those directories which
                // were matched, we want to enumerate all files inside them. This will
                // happen when the handle is empty.
                pattern_ = L"*";
            }

            continue;
        }

        const std::experimental::filesystem::path filename(find_data_.cFileName);
        if (ShouldSkip(filename))
            continue;

        const bool is_dir = (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        std::experimental::filesystem::path abs_path = root_path_;
        abs_path.append(filename);

        // Check if directory should be processed recursive.
        if (is_dir && recursive_)
        {
            // If |cur_file| is a directory, and we are doing recursive searching,
            // add it to pending_paths_ so we scan it after we finish scanning this
            // directory. However, don't do recursion through reparse points or we
            // may end up with an infinite cycle.
            DWORD attributes = GetFileAttributesW(abs_path.c_str());
            if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                pending_paths_.push(abs_path);
        }

        if (IsTypeMatched(is_dir) && IsPatternMatched(filename))
            return abs_path;
    }

    return std::experimental::filesystem::path();
}

bool FileEnumerator::ShouldSkip(const std::experimental::filesystem::path& path) const
{
    std::experimental::filesystem::path::string_type basename = path.filename();
    return basename == L"." || (basename == L".." && !(INCLUDE_DOT_DOT & file_type_));
}

bool FileEnumerator::IsTypeMatched(bool is_dir) const
{
    return (file_type_ & (is_dir ? DIRECTORIES : FILES)) != 0;
}

bool FileEnumerator::IsPatternMatched(const std::experimental::filesystem::path& src) const
{
    switch (folder_search_policy_)
    {
        case FolderSearchPolicy::MATCH_ONLY:
            // MATCH_ONLY policy filters by pattern on search request, so all found
            // files already fits to pattern.
            return true;
        case FolderSearchPolicy::ALL:
            // ALL policy enumerates all files, we need to check pattern match
            // manually.
            return !!PathMatchSpecW(src.c_str(), pattern_.c_str());
    }

    return false;
}

} // namespace aspia
