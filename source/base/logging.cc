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

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <thread>
#include <utility>

#include "base/debug.h"
#include "base/unicode.h"

#if defined(OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(OS_WIN)

namespace aspia {

namespace {

LoggingSeverity g_min_log_level = LS_INFO;
LoggingDestination g_logging_destination = LOG_DEFAULT;

std::ofstream g_log_file;
std::mutex g_log_file_lock;

const char* severityName(LoggingSeverity severity)
{
    static const char* const kLogSeverityNames[] =
    {
        "INFO", "WARNING", "ERROR", "FATAL"
    };

    static_assert(LS_NUMBER == _countof(kLogSeverityNames));

    if (severity >= 0 && severity < LS_NUMBER)
        return kLogSeverityNames[severity];

    return "UNKNOWN";
}

void removeOldFiles(const std::filesystem::path& path,
                    const std::filesystem::file_time_type& current_time,
                    int max_file_age)
{
    std::filesystem::file_time_type time = current_time - std::chrono::hours(24 * max_file_age);

    std::error_code ignored_code;
    for (const auto& item : std::filesystem::directory_iterator(path, ignored_code))
    {
        if (item.is_directory())
            continue;

        if (item.last_write_time() < time)
            std::filesystem::remove(item.path(), ignored_code);
    }
}

std::string logFileName()
{
    std::chrono::high_resolution_clock::time_point current_time =
        std::chrono::high_resolution_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time.time_since_epoch());

    time_t time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(ms).count();
    std::tm* now = std::localtime(&time_since_epoch);

    std::ostringstream stream;

    stream << std::setfill('0')
           << std::setw(4) << (1900 + now->tm_year)
           << std::setw(2) << (1 + now->tm_mon)
           << std::setw(2) << now->tm_mday
           << '-'
           << std::setw(2) << now->tm_hour
           << std::setw(2) << now->tm_min
           << std::setw(2) << now->tm_sec
           << '.'
           << std::setw(3) << (ms.count() % 1000)
           << ".log";

    return stream.str();
}

std::filesystem::path logFileDir()
{
    std::error_code error_code;

    std::filesystem::path path = std::filesystem::temp_directory_path(error_code);
    if (error_code)
        return std::filesystem::path();

    path.append("aspia");

    if (!std::filesystem::exists(path, error_code))
    {
        if (error_code)
            return std::filesystem::path();

        if (!std::filesystem::create_directories(path, error_code))
            return std::filesystem::path();
    }

    return path;
}

bool initLoggingImpl(const LoggingSettings& settings)
{
    std::scoped_lock lock(g_log_file_lock);
    g_log_file.close();

    g_logging_destination = settings.logging_dest;

    if (!(g_logging_destination & LOG_TO_FILE))
        return true;

    std::filesystem::path file_dir = logFileDir();
    if (file_dir.empty())
        return false;

    std::filesystem::path file_path(file_dir);
    file_path.append(logFileName());

    g_log_file.open(file_path);
    if (!g_log_file.is_open())
        return false;

    std::error_code error_code;

    std::filesystem::file_time_type current_time =
        std::filesystem::last_write_time(file_path, error_code);
    if (!error_code)
        removeOldFiles(file_dir, current_time, settings.max_log_age);

    return true;
}

void shutdownLoggingImpl()
{
    std::scoped_lock lock(g_log_file_lock);
    g_log_file.close();
}

} // namespace

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

LoggingSettings::LoggingSettings()
    : logging_dest(LOG_DEFAULT),
      max_log_age(7)
{
    // Nothing
}

bool initLogging(const LoggingSettings& settings)
{
    bool result = initLoggingImpl(settings);
    LOG(LS_INFO) << "Logging started";
    return result;
}

void shutdownLogging()
{
    LOG(LS_INFO) << "Logging finished";
    shutdownLoggingImpl();
}

bool shouldCreateLogMessage(LoggingSeverity severity)
{
    if (severity < g_min_log_level)
        return false;

    // Return true here unless we know ~LogMessage won't do anything. Note that
    // ~LogMessage writes to stderr if severity_ >= kAlwaysPrintErrorLevel, even
    // when g_logging_destination is LOG_NONE.
    return g_logging_destination != LOG_NONE || severity >= LS_ERROR;
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
    std::unique_ptr<std::string> result_deleter(result);
    init(file, line);
    stream_ << "Check failed: " << *result;
}

LogMessage::LogMessage(const char* file, int line, LoggingSeverity severity, std::string* result)
    : severity_(severity), file_(file), line_(line)
{
    std::unique_ptr<std::string> result_deleter(result);
    init(file, line);
    stream_ << "Check failed: " << *result;
}

LogMessage::~LogMessage()
{
    stream_ << std::endl;

    std::string message(stream_.str());

    if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0)
    {
#if defined(OS_WIN)
        OutputDebugStringA(message.c_str());
#endif // defined(OS_WIN)

        //std::cerr << str_newline << std::endl;
        fwrite(message.data(), message.size(), 1, stderr);
        fflush(stderr);
    }
    else if (severity_ >= LS_ERROR)
    {
        // When we're only outputting to a log file, above a certain log level, we
        // should still output to stderr so that we can better detect and diagnose
        // problems with unit tests, especially on the buildbots.
        fwrite(message.data(), message.size(), 1, stderr);
        fflush(stderr);
    }

    // Write to log file.
    if ((g_logging_destination & LOG_TO_FILE) != 0)
    {
        std::scoped_lock lock(g_log_file_lock);
        g_log_file.write(message.c_str(), message.size());
        g_log_file.flush();
    }

    if (severity_ == LS_FATAL)
    {
        // Crash the process.
        debugBreak();
    }
}

// Writes the common header info to the stream.
void LogMessage::init(const char* file, int line)
{
    std::string_view filename(file);

    size_t last_slash_pos = filename.find_last_of("\\/");
    if (last_slash_pos != std::string_view::npos)
        filename.remove_prefix(last_slash_pos + 1);

    std::chrono::high_resolution_clock::time_point current_time =
        std::chrono::high_resolution_clock::now();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time.time_since_epoch());

    time_t time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(ms).count();
    std::tm* now = std::localtime(&time_since_epoch);

    stream_ << std::setfill('0')
            << std::setw(2) << now->tm_hour   << ':'
            << std::setw(2) << now->tm_min << ':'
            << std::setw(2) << now->tm_sec << '.'
            << std::setw(3) << ms.count() % 1000;

    stream_ << ' ' << std::this_thread::get_id();
    stream_ << ' ' << severityName(severity_);
    stream_ << ' ' << filename.data() << ":" << line << "] ";

    message_start_ = stream_.str().length();
}

SystemErrorCode lastSystemErrorCode()
{
#if defined(OS_WIN)
    return ::GetLastError();
#elif defined(OS_POSIX)
    return errno;
#else
#error Platform support not implemented
#endif
}

std::string systemErrorCodeToString(SystemErrorCode error_code)
{
#if defined(OS_WIN)
    std::stringstream ss;

    constexpr int kErrorMessageBufferSize = 256;
    wchar_t msgbuf[kErrorMessageBufferSize];

    DWORD len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr, error_code, 0, msgbuf, _countof(msgbuf), nullptr);
    if (len)
    {
        ss << UTF8fromUTF16(msgbuf);
        ss << std::setfill('0') << " (0x" << std::hex << std::setw(8) << error_code << ")";
    }
    else
    {
        ss << "Error ";
        ss << std::setfill('0') << std::hex << std::setw(8) << GetLastError();
        ss << " while retrieving error (";
        ss << std::setfill('0') << std::hex << std::setw(8) << error_code << ")";
    }

    return ss.str();
#elif (OS_POSIX)
    return strerror(error_code);
#else
#error Platform support not implemented
#endif
}

ErrorLogMessage::ErrorLogMessage(const char* file,
                                 int line,
                                 LoggingSeverity severity,
                                 SystemErrorCode error_code)
    : error_code_(error_code),
      log_message_(file, line, severity)
{
    // Nothing
}

ErrorLogMessage::~ErrorLogMessage()
{
    stream() << ": " << systemErrorCodeToString(error_code_);
}

void logErrorNotReached(const char* file, int line)
{
    LogMessage(file, line, LS_ERROR).stream() << "NOTREACHED() hit.";
}

} // namespace aspia

std::ostream& operator<<(std::ostream& out, const std::wstring& wstr)
{
    return out << aspia::UTF8fromUTF16(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr)
{
    return out << aspia::UTF8fromUTF16(wstr);
}
