//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/files/file_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"

#include <fstream>

namespace aspia {

namespace {

const DWORD kFileShareAll =
FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

} // namespace

int64_t ComputeDirectorySize(const FilePath& root_path)
{
    int64_t running_size = 0;
    FileEnumerator file_iter(root_path, true, FileEnumerator::FILES);
    while (!file_iter.Next().empty())
        running_size += file_iter.GetInfo().GetSize();
    return running_size;
}

bool DeleteFile(const FilePath& path)
{
    if (path.empty())
        return true;

    if (path.string().length() >= MAX_PATH)
        return false;

    DWORD attr = GetFileAttributesW(path.c_str());
    // We're done if we can't find the path.
    if (attr == INVALID_FILE_ATTRIBUTES)
        return true;
    // We may need to clear the read-only bit.
    if ((attr & FILE_ATTRIBUTE_READONLY) &&
        !SetFileAttributesW(path.c_str(), attr & ~FILE_ATTRIBUTE_READONLY))
    {
        return false;
    }
    // Directories are handled differently if they're recursive.
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
        return !!::DeleteFileW(path.c_str());

    // Handle a simple, single file delete.
    return !!RemoveDirectory(path.c_str());
}

time_t UnixTimeFromFileTime(const FILETIME& file_time)
{
    ULARGE_INTEGER ull;

    ull.LowPart = file_time.dwLowDateTime;
    ull.HighPart = file_time.dwHighDateTime;

    return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

FILETIME FileTimeFromUnixTime(time_t time)
{
    LONGLONG ll = Int32x32To64(time, 10000000) + 116444736000000000;

    FILETIME file_time;

    file_time.dwLowDateTime = static_cast<DWORD>(ll);
    file_time.dwHighDateTime = ll >> 32;

    return file_time;
}

bool GetFileInfo(const FilePath& file_path, File::Info& results)
{
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExW(file_path.c_str(), GetFileExInfoStandard, &attr))
    {
        return false;
    }

    ULARGE_INTEGER size;
    size.HighPart = attr.nFileSizeHigh;
    size.LowPart = attr.nFileSizeLow;
    results.size = size.QuadPart;

    results.is_directory = (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    results.last_modified = UnixTimeFromFileTime(attr.ftLastWriteTime);
    results.last_accessed = UnixTimeFromFileTime(attr.ftLastAccessTime);
    results.creation_time = UnixTimeFromFileTime(attr.ftCreationTime);

    return true;
}

bool GetFileSize(const FilePath& file_path, int64_t& file_size)
{
    File::Info info;
    if (!GetFileInfo(file_path, info))
        return false;
    file_size = info.size;
    return true;
}

bool PathExists(const FilePath& path)
{
    return (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const FilePath& path)
{
    HANDLE dir =
        CreateFileW(path.c_str(), FILE_ADD_FILE, kFileShareAll,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (dir == INVALID_HANDLE_VALUE)
        return false;

    CloseHandle(dir);
    return true;
}

bool DirectoryExists(const FilePath& path)
{
    DWORD fileattr = GetFileAttributesW(path.c_str());
    if (fileattr != INVALID_FILE_ATTRIBUTES)
        return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return false;
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2)
{
    // We open the file in binary format even if they are text files because
    // we are just comparing that bytes are exactly same in both files and not
    // doing anything smart with text formatting.
    std::ifstream file1(filename1, std::ios::in | std::ios::binary);
    std::ifstream file2(filename2, std::ios::in | std::ios::binary);

    // Even if both files aren't openable (and thus, in some sense, "equal"),
    // any unusable file yields a result of "false".
    if (!file1.is_open() || !file2.is_open())
        return false;

    const int BUFFER_SIZE = 2056;
    char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
    do
    {
        file1.read(buffer1, BUFFER_SIZE);
        file2.read(buffer2, BUFFER_SIZE);

        if ((file1.eof() != file2.eof()) ||
            (file1.gcount() != file2.gcount()) ||
            (memcmp(buffer1, buffer2, static_cast<size_t>(file1.gcount()))))
        {
            file1.close();
            file2.close();
            return false;
        }
    }
    while (!file1.eof() || !file2.eof());

    file1.close();
    file2.close();
    return true;
}

bool ReplaceFile(const FilePath& from_path, const FilePath& to_path, File::Error* error)
{
    // Try a simple move first.  It will only succeed when |to_path| doesn't
    // already exist.
    if (::MoveFile(from_path.c_str(), to_path.c_str()))
        return true;
    File::Error move_error = File::OSErrorToFileError(GetLastError());

    // Try the full-blown replace if the move fails, as ReplaceFile will only
    // succeed when |to_path| does exist. When writing to a network share, we may
    // not be able to change the ACLs. Ignore ACL errors then
    // (REPLACEFILE_IGNORE_MERGE_ERRORS).
    if (::ReplaceFile(to_path.c_str(), from_path.c_str(), NULL,
                      REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL))
    {
        return true;
    }
    // In the case of FILE_ERROR_NOT_FOUND from ReplaceFile, it is likely that
    // |to_path| does not exist. In this case, the more relevant error comes
    // from the call to MoveFile.
    if (error)
    {
        File::Error replace_error = File::OSErrorToFileError(GetLastError());
        *error = replace_error == File::FILE_ERROR_NOT_FOUND ? move_error
            : replace_error;
    }
    return false;
}

bool CreateDirectory(const FilePath& full_path, File::Error* error)
{
    // If the path exists, we've succeeded if it's a directory, failed otherwise.
    const wchar_t* full_path_str = full_path.c_str();
    DWORD fileattr = ::GetFileAttributesW(full_path_str);
    if (fileattr != INVALID_FILE_ATTRIBUTES)
    {
        if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            DLOG(WARNING) << "CreateDirectory(" << full_path_str << "), "
                          << "directory already exists.";
            return true;
        }
        DLOG(WARNING) << "CreateDirectory(" << full_path_str << "), "
                      << "conflicts with existing file.";
        if (error)
        {
            *error = File::FILE_ERROR_NOT_A_DIRECTORY;
        }
        return false;
    }

    // Invariant:  Path does not exist as file or directory.

    // Attempt to create the parent recursively.  This will immediately return
    // true if it already exists, otherwise will create all required parent
    // directories starting with the highest-level missing parent.
    FilePath parent_path(full_path.parent_path());
    if (parent_path == full_path)
    {
        if (error)
        {
            *error = File::FILE_ERROR_NOT_FOUND;
        }
        return false;
    }
    if (!CreateDirectory(parent_path, error))
    {
        DLOG(WARNING) << "Failed to create one of the parent directories.";
        if (error)
        {
            DCHECK(*error != File::FILE_OK);
        }
        return false;
    }

    if (!::CreateDirectory(full_path_str, NULL))
    {
        DWORD error_code = ::GetLastError();
        if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path))
        {
            // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we
            // were racing with someone creating the same directory, or a file
            // with the same path.  If DirectoryExists() returns true, we lost the
            // race to create the same directory.
            return true;
        }
        else
        {
            if (error)
                *error = File::OSErrorToFileError(error_code);
            DLOG(WARNING) << "Failed to create directory " << full_path_str
                          << ", last error is " << error_code << ".";
            return false;
        }
    }
    else
    {
        return true;
    }
}

} // namespace aspia
