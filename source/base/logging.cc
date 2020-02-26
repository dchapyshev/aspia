//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/debug.h"
#include "base/system_time.h"

#if defined(OS_WIN)
#include "base/strings/unicode.h"
#endif // defined(OS_WIN)

#include <fstream>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <thread>

#if defined(OS_WIN)
#include <Windows.h>
#include <Psapi.h>
#endif // defined(OS_WIN)

namespace base {

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

    static_assert(LS_NUMBER == std::size(kLogSeverityNames));

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

std::filesystem::path defaultLogFileDir()
{
    std::error_code error_code;

    std::filesystem::path path = std::filesystem::temp_directory_path(error_code);
    if (error_code)
        return std::filesystem::path();

    return path;
}

bool initLoggingImpl(const LoggingSettings& settings)
{
    std::scoped_lock lock(g_log_file_lock);
    g_log_file.close();

    g_logging_destination = settings.destination;

    if (!(g_logging_destination & LOG_TO_FILE))
        return true;

    std::filesystem::path file_dir = settings.log_dir;

    if (file_dir.empty())
        file_dir = defaultLogFileDir();

    if (file_dir.empty())
        return false;

    std::error_code error_code;
    if (!std::filesystem::exists(file_dir, error_code))
    {
        if (error_code)
            return false;

        if (!std::filesystem::create_directories(file_dir, error_code))
            return false;
    }

    SystemTime time = SystemTime::now();

    std::ostringstream file_name_stream;
    file_name_stream << std::setfill('0')
                     << std::setw(4) << time.year()
                     << std::setw(2) << time.month()
                     << std::setw(2) << time.day()
                     << '-'
                     << std::setw(2) << time.hour()
                     << std::setw(2) << time.minute()
                     << std::setw(2) << time.second()
                     << '.'
                     << std::setw(3) << time.millisecond()
                     << ".log";

    std::filesystem::path file_path(file_dir);
    file_path.append(file_name_stream.str());

    g_log_file.open(file_path);
    if (!g_log_file.is_open())
        return false;

    std::filesystem::file_time_type file_time =
        std::filesystem::last_write_time(file_path, error_code);
    if (!error_code)
        removeOldFiles(file_dir, file_time, settings.max_log_age);

    return true;
}

} // namespace

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

LoggingSettings::LoggingSettings()
    : destination(LOG_DEFAULT),
      min_log_level(LS_INFO),
      max_log_age(7)
{
    // Nothing
}

bool initLogging(const LoggingSettings& settings)
{
    if (!initLoggingImpl(settings))
        return false;

#if defined(OS_WIN)
    wchar_t buffer[MAX_PATH] = { 0 };

    if (GetModuleFileNameExW(GetCurrentProcess(), nullptr, buffer, std::size(buffer)))
    {
        LOG(LS_INFO) << "Executable file: " << buffer;
    }

#if defined(NDEBUG)
    LOG(LS_INFO) << "Debug build: No";
#else
    LOG(LS_INFO) << "Debug build: Yes";
#endif // defined(NDEBUG)
#else
    #warning Not implemented
#endif

    LOG(LS_INFO) << "Logging started";
    return true;
}

void shutdownLogging()
{
    LOG(LS_INFO) << "Logging finished";

    std::scoped_lock lock(g_log_file_lock);
    g_log_file.close();
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
        debugPrint(message.data());

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

    SystemTime time = SystemTime::now();

    stream_ << std::setfill('0')
            << std::setw(2) << time.hour()        << ':'
            << std::setw(2) << time.minute()      << ':'
            << std::setw(2) << time.second()      << '.'
            << std::setw(3) << time.millisecond() << ' '
            << std::this_thread::get_id()         << ' '
            << severityName(severity_)            << ' '
            << filename.data() << ":" << line << "] ";

    message_start_ = stream_.str().length();
}

ErrorLogMessage::ErrorLogMessage(const char* file,
                                 int line,
                                 LoggingSeverity severity,
                                 SystemError error)
    : error_(error),
      log_message_(file, line, severity)
{
    // Nothing
}

ErrorLogMessage::~ErrorLogMessage()
{
    stream() << ": " << error_.toString();
}

void logErrorNotReached(const char* file, int line)
{
    LogMessage(file, line, LS_ERROR).stream() << "NOTREACHED() hit.";
}

} // namespace base

namespace std {

#if defined(OS_WIN)
std::ostream& operator<<(std::ostream& out, const std::wstring& wstr)
{
    return out << base::utf8FromWide(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr)
{
    return out << (wstr ? base::utf8FromWide(wstr) : "nullptr");
}
#endif // defined(OS_WIN)

std::ostream& operator<<(std::ostream& out, const char16_t* ustr)
{
    return out << (ustr ? base::utf8FromUtf16(ustr) : "nullptr");
}

std::ostream& operator<<(std::ostream& out, const std::u16string& ustr)
{
    return out << base::utf8FromUtf16(ustr);
}

} // namespace std
