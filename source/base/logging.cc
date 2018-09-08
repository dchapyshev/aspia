//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"

#include <io.h>
#include <algorithm>
#include <iomanip>
#include <ostream>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "base/string_util.h"
#include "base/string_printf.h"
#include "base/unicode.h"

// Windows warns on using write().  It prefers _write().
#define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2

namespace aspia {

namespace {

using FileHandle = HANDLE;

LoggingSeverity g_min_log_level = LS_INFO;
LoggingDestination g_logging_destination = LOG_DEFAULT;

// For LS_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LS_ERROR;

std::filesystem::path g_log_file_name;

// This file is lazily opened and the handle may be nullptr
FileHandle g_log_file = nullptr;

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction log_message_handler = nullptr;

const char* GetSeverityName(LoggingSeverity severity)
{
    static const char* const kLogSeverityNames[] = {"INFO", "WARNING", "ERROR", "FATAL"};
    static_assert(LS_NUMBER == _countof(kLogSeverityNames));

    if (severity >= 0 && severity < LS_NUMBER)
        return kLogSeverityNames[severity];
    return "UNKNOWN";
}

bool defaultLogFilePath(std::filesystem::path* path)
{
    std::error_code code;

    *path = std::filesystem::temp_directory_path(code);
    if (code.value() != 0)
        return false;

    path->append(L"aspia");

    if (!std::filesystem::exists(*path, code))
    {
        if (code.value() != 0)
            return false;

        if (!std::filesystem::create_directories(*path, code))
            return false;
    }

    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    std::ostringstream stream;

    stream << std::setfill('0')
           << std::setw(2) << local_time.wYear
           << std::setw(2) << local_time.wMonth
           << std::setw(2) << local_time.wDay
           << '-'
           << std::setw(2) << local_time.wHour
           << std::setw(2) << local_time.wMinute
           << std::setw(2) << local_time.wSecond
           << '.'
           << std::setw(3) << local_time.wMilliseconds
           << '.'
           << GetCurrentProcessId()
           << ".log";

    path->append(stream.str());

    return true;
}

// Called by logging functions to ensure that |g_log_file| is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. |g_log_file| will be nullptr in this case.
bool initializeLogFileHandle()
{
    if (g_log_file)
        return true;

    if (g_log_file_name.empty())
    {
        if (!defaultLogFilePath(&g_log_file_name))
            return false;
    }

    if ((g_logging_destination & LOG_TO_FILE) != 0)
    {
        // The FILE_APPEND_DATA access mask ensures that the file is atomically
        // appended to across accesses from multiple threads.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
        // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
        g_log_file = CreateFileW(g_log_file_name.c_str(), FILE_APPEND_DATA,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr)
        {
            g_log_file = nullptr;
            return false;
        }
    }

    return true;
}

void closeLogFileUnlocked()
{
    if (!g_log_file)
        return;

    LARGE_INTEGER file_size;

    BOOL ret = GetFileSizeEx(g_log_file, &file_size);

    CloseHandle(g_log_file);
    g_log_file = nullptr;

    if (ret && !file_size.QuadPart)
    {
        // If the file size was successfully received and nothing was written to the file,
        // then delete it.
        DeleteFileW(g_log_file_name.c_str());
    }
}

} // namespace

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

LoggingSettings::LoggingSettings()
    : logging_dest(LOG_DEFAULT),
      lock_log(LOCK_LOG_FILE)
{
    // Nothing
}

bool baseInitLoggingImpl(const LoggingSettings& settings)
{
    g_logging_destination = settings.logging_dest;

    // ignore file options unless logging to file is set.
    if ((g_logging_destination & LOG_TO_FILE) == 0)
        return true;

    // Calling InitLogging twice or after some log call has already opened the
    // default log file will re-initialize to the new options.
    closeLogFileUnlocked();

    return initializeLogFileHandle();
}

void setMinLogLevel(LoggingSeverity level)
{
    g_min_log_level = std::min(LS_FATAL, level);
}

LoggingSeverity minLogLevel()
{
    return g_min_log_level;
}

bool shouldCreateLogMessage(LoggingSeverity severity)
{
    if (severity < g_min_log_level)
        return false;

    // Return true here unless we know ~LogMessage won't do anything. Note that
    // ~LogMessage writes to stderr if severity_ >= kAlwaysPrintErrorLevel, even
    // when g_logging_destination is LOG_NONE.
    return g_logging_destination != LOG_NONE || log_message_handler ||
           severity >= kAlwaysPrintErrorLevel;
}

void setLogMessageHandler(LogMessageHandlerFunction handler)
{
    log_message_handler = handler;
}

LogMessageHandlerFunction logMessageHandler()
{
    return log_message_handler;
}

void makeCheckOpValueString(std::ostream* os, std::nullptr_t /* p */)
{
    (*os) << "nullptr";
}

std::string* makeCheckOpString(const int& v1, const int& v2, const char* names)
{
    return makeCheckOpString<int, int>(v1, v2, names);
}

std::string* makeCheckOpString(const unsigned long& v1, const unsigned long& v2, const char* names)
{
    return makeCheckOpString<unsigned long, unsigned long>(v1, v2, names);
}

std::string* makeCheckOpString(const unsigned int& v1, const unsigned int& v2, const char* names)
{
    return makeCheckOpString<unsigned int, unsigned int>(v1, v2, names);
}

std::string* makeCheckOpString(const unsigned long long& v1,
                               const unsigned long long& v2,
                               const char* names)
{
    return makeCheckOpString<unsigned long long, unsigned long long>(v1, v2, names);
}

std::string* makeCheckOpString(const unsigned long& v1, const unsigned int& v2, const char* names)
{
    return makeCheckOpString<unsigned long, int>(v1, v2, names);
}

std::string* makeCheckOpString(const unsigned int& v1, const unsigned long& v2, const char* names)
{
    return makeCheckOpString<unsigned int, unsigned long>(v1, v2, names);
}

std::string* makeCheckOpString(const std::string& v1, const std::string& v2, const char* names)
{
    return makeCheckOpString<std::string, std::string>(v1, v2, names);
}

LogMessage::SaveLastError::SaveLastError()
    : last_error_(::GetLastError())
{
    // Nothing
}

LogMessage::SaveLastError::~SaveLastError()
{
    ::SetLastError(last_error_);
}

LogMessage::LogMessage(const char* file, int line, LoggingSeverity severity)
    : severity_(severity), file_(file), line_(line)
{
    init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LS_FATAL), file_(file), line_(line)
{
    init(file, line);
    stream_ << "Check failed: " << condition << ". ";
}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LS_FATAL), file_(file), line_(line)
{
    init(file, line);
    stream_ << "Check failed: " << *result;
    delete result;
}

LogMessage::LogMessage(const char* file, int line, LoggingSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line)
{
    init(file, line);
    stream_ << "Check failed: " << *result;
    delete result;
}

LogMessage::~LogMessage()
{
    stream_ << std::endl;
    std::string str_newline(stream_.str());

    // Give any log message handler first dibs on the message.
    if (log_message_handler &&
        log_message_handler(severity_, file_, line_, message_start_, str_newline))
    {
        // The handler took care of it, no further processing.
        return;
    }

    if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0)
    {
        OutputDebugStringA(str_newline.c_str());

        fwrite(str_newline.data(), str_newline.size(), 1, stderr);
        fflush(stderr);
    }
    else if (severity_ >= kAlwaysPrintErrorLevel)
    {
        // When we're only outputting to a log file, above a certain log level, we
        // should still output to stderr so that we can better detect and diagnose
        // problems with unit tests, especially on the buildbots.
        fwrite(str_newline.data(), str_newline.size(), 1, stderr);
        fflush(stderr);
    }

    // write to log file
    if ((g_logging_destination & LOG_TO_FILE) != 0)
    {
        // We can have multiple threads and/or processes, so try to prevent them
        // from clobbering each other's writes.
        // If the client app did not call InitLogging, and the lock has not
        // been created do it now. We do this on demand, but if two threads try
        // to do this at the same time, there will be a race condition to create
        // the lock. This is why InitLogging should be called from the main
        // thread at the beginning of execution.

        if (initializeLogFileHandle())
        {
            DWORD num_written;
            WriteFile(g_log_file,
                      static_cast<const void*>(str_newline.c_str()),
                      static_cast<DWORD>(str_newline.length()),
                      &num_written,
                      nullptr);
        }
    }

    if (severity_ == LS_FATAL)
    {
        // Crash the process.
        __debugbreak();
    }
}

// writes the common header info to the stream
void LogMessage::init(const char* file, int line)
{
    std::string_view filename(file);

    size_t last_slash_pos = filename.find_last_of("\\/");
    if (last_slash_pos != std::string_view::npos)
        filename.remove_prefix(last_slash_pos + 1);

    stream_ <<  '[';
    stream_ << GetCurrentThreadId() << ':';

    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    stream_ << std::setfill('0')
            << std::setw(2) << local_time.wMonth
            << std::setw(2) << local_time.wDay
            << '/'
            << std::setw(2) << local_time.wHour
            << std::setw(2) << local_time.wMinute
            << std::setw(2) << local_time.wSecond
            << '.'
            << std::setw(3) << local_time.wMilliseconds
            << ':';

    stream_ << GetSeverityName(severity_);
    stream_ << ":" << filename.data() << ":" << line << "] ";

    message_start_ = stream_.str().length();
}

SystemErrorCode lastSystemErrorCode()
{
    return ::GetLastError();
}

std::string systemErrorCodeToString(SystemErrorCode error_code)
{
    constexpr int kErrorMessageBufferSize = 256;
    char msgbuf[kErrorMessageBufferSize];

    static const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf, _countof(msgbuf), nullptr);
    if (len)
    {
        // Messages returned by system end with line breaks.
        return aspia::collapseWhitespaceASCII(msgbuf, true) +
               aspia::stringPrintf(" (0x%lX)", error_code);
    }

    return aspia::stringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                               GetLastError(), error_code);
}

Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity)
{
    // Nothing
}

Win32ErrorLogMessage::~Win32ErrorLogMessage()
{
    stream() << ": " << systemErrorCodeToString(err_);
}

void shutdownLogging()
{
    closeLogFileUnlocked();
}

void rawLog(int level, const char* message)
{
    if (level >= g_min_log_level && message)
    {
        size_t bytes_written = 0;
        const size_t message_len = strlen(message);
        int rv;
        while (bytes_written < message_len)
        {
            rv = write(STDERR_FILENO,
                       message + bytes_written,
                       message_len - bytes_written);
            if (rv < 0)
            {
                // Give up, nothing we can do now.
                break;
            }
            bytes_written += rv;
        }

        if (message_len > 0 && message[message_len - 1] != '\n')
        {
            do
            {
                rv = write(STDERR_FILENO, "\n", 1);
                if (rv < 0)
                {
                    // Give up, nothing we can do now.
                    break;
                }
            }
            while (rv != 1);
        }
    }

    if (level == LS_FATAL)
        __debugbreak();
}

bool isLoggingToFileEnabled()
{
    return g_logging_destination & LOG_TO_FILE;
}

std::filesystem::path logFileFullPath()
{
    return g_log_file_name;
}

void logErrorNotReached(const char* file, int line)
{
    LogMessage(file, line, LS_ERROR).stream() << "NOTREACHED() hit.";
}

} // namespace aspia

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr)
{
    return out << (wstr ? aspia::UTF8fromUTF16(wstr) : std::string());
}
