//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/file_util.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__FILES__FILE_UTIL_H
#define _ASPIA_BASE__FILES__FILE_UTIL_H

#include "base/files/file_path.h"

namespace aspia {

// Returns the total number of bytes used by all the files under |root_path|.
// If the path does not exist the function returns 0.
//
// This function is implemented using the FileEnumerator class so it is not
// particularly speedy in any platform.
int64_t ComputeDirectorySize(const FilePath& root_path);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise. It is considered successful
// to attempt to delete a file that does not exist.
//
// In posix environment and if |path| is a symbolic link, this deletes only
// the symlink. (even if the symlink points to a non-existent file)
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
bool DeleteFile(const FilePath& path);

time_t UnixTimeFromFileTime(const FILETIME& file_time);

struct FileInfo
{
    // The size of the file in bytes.  Undefined when is_directory is true.
    int64_t size = 0;

    // True if the file corresponds to a directory.
    bool is_directory = false;

    // True if the file corresponds to a symbolic link.  For Windows currently
    // not supported and thus always false.
    bool is_symbolic_link = false;

    // The last modified time of a file.
    time_t last_modified = 0;

    // The last accessed time of a file.
    time_t last_accessed = 0;

    // The creation time of a file.
    time_t creation_time = 0;
};

// Returns information about the given file path.
bool GetFileInfo(const FilePath& file_path, FileInfo& info);

bool GetFileSize(const FilePath& file_path, int64_t& file_size);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const FilePath& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
bool DirectoryExists(const FilePath& path);

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
bool ContentsEqual(const FilePath& filename1, const FilePath& filename2);

} // namespace aspia

#endif // _ASPIA_BASE__FILES__FILE_UTIL_H
