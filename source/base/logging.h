//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_LOGGING_H
#define BASE_LOGGING_H

#include <QRect>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "base/scoped_clear_last_error.h"
#include "base/system_error.h"

#include <filesystem>
#include <type_traits>
#include <utility>

// Instructions
// ------------
//
// Make a bunch of macros for logging. The way to log things is to stream things to
// LOG(<a particular severity level>). E.g.,
//
//   LOG(LS_INFO) << "Found " << num_cookies << " cookies";
//
// You can also do conditional logging:
//
//   LOG_IF(LS_INFO, num_cookies > 10) << "Got lots of cookies";
//
// The CHECK(condition) macro is active in both debug and release builds and effectively performs a
// LOG(LS_FATAL) which terminates the process and generates a crashdump unless a debugger is attached.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(LS_INFO) << "Found cookies";
//
//   DLOG_IF(LS_INFO, num_cookies > 10) << "Got lots of cookies";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode compiles. LOG_IF and
// development flags also work well together because the code can be compiled away sometimes.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(LS_FATAL, assert fails) << assertion;
//
// We also override the standard 'assert' to use 'DLOG_ASSERT'.
//
// Lastly, there is:
//
//   PLOG(LS_ERROR) << "Couldn't do foo";
//   DPLOG(LS_ERROR) << "Couldn't do foo";
//   PLOG_IF(LS_ERROR, cond) << "Couldn't do foo";
//   DPLOG_IF(LS_ERROR, cond) << "Couldn't do foo";
//   PCHECK(condition) << "Couldn't do foo";
//   DPCHECK(condition) << "Couldn't do foo";
//
// which append the last system error to the message in string form (taken from GetLastError() on
// Windows and errno on POSIX).
//
// The supported severity levels for macros that allow you to specify one are (in increasing order
// of severity) LS_INFO, LS_WARNING, LS_ERROR, and LS_FATAL.
//
// Very important: logging a message at the LS_FATAL severity level causes the program to terminate
// (after the message is logged).
//
// There is the special severity of DFATAL, which logs LS_FATAL in debug mode, LS_ERROR in normal mode.

namespace base {

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() false
#else
#define DCHECK_IS_ON() true
#endif

// Where to record logging output? A flat file and/or system debug log via OutputDebugString.
enum LoggingDestination
{
    LOG_NONE      = 0,
    LOG_TO_FILE   = 1 << 0,
    LOG_TO_STDOUT = 1 << 1,

    LOG_TO_ALL    = LOG_TO_FILE | LOG_TO_STDOUT,

#if defined(Q_OS_WINDOWS)
    // On Windows, use a file next to the exe.
#if defined(NDEBUG)
    LOG_DEFAULT = LOG_TO_FILE
#else
    LOG_DEFAULT = LOG_TO_ALL
#endif
#elif defined(Q_OS_UNIX)
    LOG_DEFAULT = LOG_TO_STDOUT
#endif
};

using LoggingSeverity = int;

[[maybe_unused]] const LoggingSeverity LOG_LS_INFO = 0;
[[maybe_unused]] const LoggingSeverity LOG_LS_WARNING = 1;
[[maybe_unused]] const LoggingSeverity LOG_LS_ERROR = 2;
[[maybe_unused]] const LoggingSeverity LOG_LS_FATAL = 3;
[[maybe_unused]] const LoggingSeverity LOG_LS_NUMBER = 4;
[[maybe_unused]] const LoggingSeverity LOG_LS_DFATAL = LOG_LS_FATAL;
[[maybe_unused]] const LoggingSeverity LOG_LS_DCHECK = LOG_LS_FATAL;

struct LoggingSettings
{
    // The defaults values are:
    //
    //  destination: LOG_DEFAULT
    //  min_log_level: LOG_LS_INFO
    //  max_log_file_size: 2 Mb
    //  max_log_file_age: 14 days
    LoggingSettings();

    LoggingDestination destination;
    LoggingSeverity min_log_level;

    QString log_dir;

    qint64 max_log_file_size;
    qint64 max_log_file_age;
};

// Sets the log file name and other global logging state. Calling this function is recommended,
// and is normally done at the beginning of application init.
// See the definition of the enums above for descriptions and default values.
bool initLogging(const LoggingSettings& settings = LoggingSettings());

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging statements, there's no
//       guarantee that it will stay closed after this call.
void shutdownLogging();

QString loggingDirectory();
QString loggingFile();

// Used by LOG_IS_ON to lazy-evaluate stream arguments.
bool shouldCreateLogMessage(LoggingSeverity severity);

// A few definitions of macros that don't generate much code. These are used by LOG() and LOG_IF,
// etc. Since these are used all over our code, it's better to have compact code for these operations.
#define COMPACT_LOG_EX_LS_INFO(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_INFO, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_WARNING(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_WARNING, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_ERROR(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_ERROR, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_FATAL(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_FATAL, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_DFATAL(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_DFATAL, ##__VA_ARGS__)
#define COMPACT_LOG_EX_LS_DCHECK(ClassName, ...) \
    ::base::ClassName(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_DCHECK, ##__VA_ARGS__)

#define COMPACT_LOG_LS_INFO    COMPACT_LOG_EX_LS_INFO(LogMessage)
#define COMPACT_LOG_LS_WARNING COMPACT_LOG_EX_LS_WARNING(LogMessage)
#define COMPACT_LOG_LS_ERROR   COMPACT_LOG_EX_LS_ERROR(LogMessage)
#define COMPACT_LOG_LS_FATAL   COMPACT_LOG_EX_LS_FATAL(LogMessage)
#define COMPACT_LOG_LS_DFATAL  COMPACT_LOG_EX_LS_DFATAL(LogMessage)
#define COMPACT_LOG_LS_DCHECK  COMPACT_LOG_EX_LS_DCHECK(LogMessage)

// As special cases, we can assume that LOG_IS_ON(LS_FATAL) always holds. Also, LOG_IS_ON(LS_DFATAL)
// always holds in debug mode. In particular, CHECK()s will always fire if they fail.
#define LOG_IS_ON(severity) \
    (::base::shouldCreateLogMessage(::base::LOG_##severity))

// Helper macro which avoids evaluating the arguments to a stream if the condition doesn't hold.
// Condition is evaluated once and only once.
#define LAZY_STREAM(stream, condition) \
  !(condition) ? (void) 0 : ::base::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g., LOG(LS_INFO) becomes the token
// COMPACT_LOG_LS_INFO. There's some funny subtle difference between ostream member streaming
// functions (e.g., ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's impossible to stream something
// like a string directly to an unnamed ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) COMPACT_LOG_ ## severity.stream()

#define LOG(severity) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition) \
  LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

#define LOG_ASSERT(condition) \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition ". "

#define PLOG_STREAM(severity) \
  COMPACT_LOG_EX_ ## severity(ErrorLogMessage, ::base::SystemError::last()).stream()

#define PLOG(severity) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))

#define PLOG_IF(severity, condition) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

extern QTextStream* g_swallow_stream;

// Note that g_swallow_stream is used instead of an arbitrary LOG() stream to avoid the creation of
// an object with a non-trivial destructor (LogMessage).
// On MSVC x86 (checked on 2015 Update 3), this causes a few additional pointless instructions to
// be emitted even at full optimization level, even though the : arm of the ternary operator is
// clearly never executed. Using a simpler object to be &'d with Voidify() avoids these extra
// instructions.
// Using a simpler POD object with a templated operator<< also works to avoid these instructions.
// However, this causes warnings on statically defined implementations of operator<<(QTextStream, ...)
// in some .cc files, because they become defined-but-unreferenced functions. A reinterpret_cast of
// 0 to an ostream* also is not suitable, because some compilers warn of undefined behavior.
#define EAT_STREAM_PARAMETERS \
  true ? (void)0 : ::base::LogMessageVoidify() & (*::base::g_swallow_stream)

// Captures the result of a CHECK_EQ (for example) and facilitates testing as a boolean.
class CheckOpResult
{
public:
    // |message| must be non-null if and only if the check failed.
    constexpr CheckOpResult(QString* message)
        : message_(message)
    {
        // Nothing
    }

    // Returns true if the check succeeded.
    constexpr operator bool() const { return !message_; }
    // Returns the message.
    QString* message() { return message_; }

private:
    QString* message_;
};

// CHECK dies with a fatal error if condition is not true.  It is *not* controlled by NDEBUG, so
// the check will be executed regardless of compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as doing
// CHECK(FunctionWithSideEffect()) is a common idiom.

// Do as much work as possible out of line to reduce inline code size.
#define CHECK(condition)                                                                         \
    LAZY_STREAM(::base::LogMessage(__FILE__, __LINE__, __FUNCTION__, #condition).stream(), !(condition))

#define PCHECK(condition)                                                                        \
    LAZY_STREAM(PLOG_STREAM(LS_FATAL), !(condition)) << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the macro is used in an
// 'if' clause such as:
// if (a == 1)
//   CHECK_EQ(2, a);
#define CHECK_OP(name, op, val1, val2)                                                           \
  switch (0) case 0: default:                                                                    \
  if (::base::CheckOpResult true_if_passed =                                                     \
      ::base::check##name##Impl((val1), (val2), #val1 " " #op " " #val2));                       \
  else                                                                                           \
      ::base::LogMessage(__FILE__, __LINE__, __FUNCTION__, true_if_passed.message()).stream()

template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};

template <typename T>
struct SupportsOstreamOperator<T, decltype(
    void(std::declval<QTextStream&>() << std::declval<T>()))> : std::true_type {};

template<typename T>
inline constexpr bool SupportsOstreamOperator_v = SupportsOstreamOperator<T>::value;

// This formats a value for a failing CHECK_XX statement. Ordinarily, it uses the definition for
// operator<<, with a few special cases below.
template <typename T>
inline std::enable_if_t<
    SupportsOstreamOperator_v<const T&> && !std::is_function_v<std::remove_pointer_t<T>>, void>
makeCheckOpValueString(QTextStream* os, const T& v)
{
    (*os) << v;
}

// Provide an overload for functions and function pointers. Function pointers don't implicitly
// convert to void* but do implicitly convert to bool, so without this function pointers are always
// printed as 1 or 0. (MSVC isn't standards-conforming here and converts function pointers to regular
// pointers, so this is a no-op for MSVC.)
template <typename T>
inline std::enable_if_t<std::is_function_v<std::remove_pointer_t<T>>, void>
makeCheckOpValueString(QTextStream* os, const T& v)
{
    (*os) << reinterpret_cast<const void*>(v);
}

// We need overloads for enums that don't support operator<<. (i.e. scoped enums where no
// operator<< overload was declared).
template <typename T>
inline std::enable_if_t<!SupportsOstreamOperator_v<const T&> && std::is_enum_v<T>, void>
makeCheckOpValueString(QTextStream* os, const T& v)
{
    (*os) << static_cast<std::underlying_type_t<T>>(v);
}

// We need an explicit overload for std::nullptr_t.
void makeCheckOpValueString(QTextStream* os, std::nullptr_t p);

// Build the error message string.  This is separate from the "Impl" function template because it
// is not performance critical and so can be out of line, while the "Impl" code should be inline.
// Caller takes ownership of the returned string.
template<class t1, class t2>
QString* makeCheckOpString(const t1& v1, const t2& v2, const char* names)
{
    QString string;

    {
        QTextStream ss(&string);
        ss << names << " (";
        makeCheckOpValueString(&ss, v1);
        ss << " vs. ";
        makeCheckOpValueString(&ss, v2);
        ss << ")";
    }

    QString* msg = new QString(string);
    return msg;
}

// Commonly used instantiations of makeCheckOpString<>. Explicitly instantiated in logging.cc.
QString* makeCheckOpString(const int& v1, const int& v2, const char* names);
QString* makeCheckOpString(const unsigned long& v1, const unsigned long& v2, const char* names);
QString* makeCheckOpString(const unsigned int& v1, const unsigned int& v2, const char* names);
QString* makeCheckOpString(const unsigned long long& v1, const unsigned long long& v2, const char* names);
QString* makeCheckOpString(const unsigned long& v1, const unsigned int& v2, const char* names);
QString* makeCheckOpString(const unsigned int& v1, const unsigned long& v2, const char* names);
QString* makeCheckOpString(const QString& v1, const QString& v2, const char* names);

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler will not instantiate the
// template version of the function on values of unnamed enum type - see comment below.
#define DEFINE_CHECK_OP_IMPL(name, op)                                                           \
    template <class t1, class t2>                                                                \
    constexpr QString* check##name##Impl(const t1& v1, const t2& v2, const char* names)          \
    {                                                                                            \
        if ((v1 op v2))                                                                          \
            return nullptr;                                                                      \
        else                                                                                     \
            return ::base::makeCheckOpString(v1, v2, names);                                     \
    }                                                                                            \
    constexpr QString* check##name##Impl(int v1, int v2, const char* names)                      \
    {                                                                                            \
        if ((v1 op v2))                                                                          \
            return nullptr;                                                                      \
        else                                                                                     \
            return ::base::makeCheckOpString(v1, v2, names);                                     \
    }

DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)

// Definitions for DLOG et al.

#if DCHECK_IS_ON()

#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)
#define DPLOG_IF(severity, condition) PLOG_IF(severity, condition)

#else // DCHECK_IS_ON()

// If !DCHECK_IS_ON(), we want to avoid emitting any references to |condition| (which may reference
// a variable defined only if DCHECK_IS_ON()).
// Contrast this with DCHECK et al., which has different behavior.

#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DLOG_ASSERT(condition) EAT_STREAM_PARAMETERS
#define DPLOG_IF(severity, condition) EAT_STREAM_PARAMETERS

#endif // DCHECK_IS_ON()

#define DLOG(severity) LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))
#define DPLOG(severity) LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))

// Definitions for DCHECK et al.

// DCHECK et al. make sure to reference |condition| regardless of whether DCHECKs are enabled; this
// is so that we don't get unused variable warnings if the only use of a variable is in a DCHECK.
// This behavior is different from DLOG_IF et al.
//
// Note that the definition of the DCHECK macros depends on whether or not DCHECK_IS_ON() is true.
// When DCHECK_IS_ON() is false, the macros use EAT_STREAM_PARAMETERS to avoid expressions that
// would create temporaries.

#if DCHECK_IS_ON()

#define DCHECK(condition) \
    LAZY_STREAM(LOG_STREAM(LS_DCHECK), !(condition)) << "Check failed: " #condition ". "
#define DPCHECK(condition) \
    LAZY_STREAM(PLOG_STREAM(LS_DCHECK), !(condition)) << "Check failed: " #condition ". "

#else // DCHECK_IS_ON()

#define DCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)
#define DPCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)

#endif // DCHECK_IS_ON()

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the macro is used in an
// 'if' clause such as:
// if (a == 1)
//   DCHECK_EQ(2, a);
#if DCHECK_IS_ON()

#define DCHECK_OP(name, op, val1, val2)                                                          \
    switch (0) case 0: default:                                                                  \
    if (::base::CheckOpResult true_if_passed =                                                   \
        DCHECK_IS_ON() ?                                                                         \
        ::base::check##name##Impl((val1), (val2),  #val1 " " #op " " #val2) : nullptr);          \
    else                                                                                         \
        ::base::LogMessage(__FILE__, __LINE__, __FUNCTION__, ::base::LOG_LS_DCHECK,              \
                           true_if_passed.message()).stream()

#else // DCHECK_IS_ON()

// When DCHECKs aren't enabled, DCHECK_OP still needs to reference operator<< overloads for |val1|
// and |val2| to avoid potential compiler warnings about unused functions. For the same reason, it
// also compares |val1| and |val2| using |op|.
//
// Note that the contract of DCHECK_EQ, etc is that arguments are only evaluated once. Even though
// |val1| and |val2| appear twice in this version of the macro expansion, this is OK, since the
// expression is never actually evaluated.
#define DCHECK_OP(name, op, val1, val2)                                                          \
  EAT_STREAM_PARAMETERS << (::base::makeCheckOpValueString(::base::g_swallow_stream, val1),      \
                            ::base::makeCheckOpValueString(::base::g_swallow_stream, val2),      \
                            (val1)op(val2))

#endif // DCHECK_IS_ON()

// Equality/Inequality checks - compare two values, and log a LOG_DCHECK message including the two
// values when the result is not as expected.  The values must have operator<<(ostream, ...)
// defined.
//
// You may append to the error message like so:
//   DCHECK_NE(1, 2) << "The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly once, and that anything
// which is legal to pass as a function argument is legal here.  In particular, the arguments may
// be temporary expressions which will end up being destroyed at the end of the apparent statement,
// for example:
//   DCHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer and the other is NULL.
// In new code, prefer nullptr instead.  To work around this for C++98, simply static_cast NULL to
// the type of the desired pointer.

#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)

#define NOTREACHED() DCHECK(false)

// This class more or less represents a particular log message. You create an instance of LogMessage
// and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the full message gets streamed to the
// appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things, though.  You should use the
// LOG() macro (and variants thereof) above.
class LogMessage
{
public:
    // Used for LOG(severity).
    LogMessage(std::string_view file, int line, std::string_view function,
               LoggingSeverity severity);

    // Used for CHECK(). Implied severity = LOG_FATAL.
    LogMessage(std::string_view file, int line, std::string_view function, const char* condition);

    // Used for CHECK_EQ(), etc. Takes ownership of the given string.
    // Implied severity = LOG_FATAL.
    LogMessage(std::string_view file, int line, std::string_view function, QString* result);

    // Used for DCHECK_EQ(), etc. Takes ownership of the given string.
    LogMessage(std::string_view file, int line, std::string_view function,
               LoggingSeverity severity, QString* result);

    ~LogMessage();

    QTextStream& stream() { return stream_; }

    LoggingSeverity severity() { return severity_; }

private:
    void init(std::string_view file, int line, std::string_view function);

    LoggingSeverity severity_;
    QString string_;
    QTextStream stream_;

    ScopedClearLastError last_error_;

    DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

// This class is used to explicitly ignore values in the conditional logging macros. This avoids
// compiler warnings like "value computed is not used" and "statement has no effect".
class LogMessageVoidify
{
public:
    LogMessageVoidify() = default;
    // This has to be an operator with a precedence lower than << but higher than ?:
    void operator&(QTextStream&) { }
};

// Appends a formatted system message of the GetLastError() type.
class ErrorLogMessage
{
public:
    ErrorLogMessage(std::string_view file, int line, std::string_view function,
                    LoggingSeverity severity, SystemError error);

    // Appends the error message before destructing the encapsulated class.
    ~ErrorLogMessage();

    QTextStream& stream() { return log_message_.stream(); }

private:
    SystemError error_;
    LogMessage log_message_;

    DISALLOW_COPY_AND_ASSIGN(ErrorLogMessage);
};

class ScopedLogging
{
public:
    ScopedLogging(const LoggingSettings& settings = LoggingSettings())
    {
        initialized_ = initLogging(settings);
    }

    ~ScopedLogging()
    {
        if (initialized_)
            shutdownLogging();
    }

private:
    bool initialized_ = false;
};

} // namespace base

#if defined(Q_OS_WINDOWS)
QTextStream& operator<<(QTextStream& out, const wchar_t* wstr);
QTextStream& operator<<(QTextStream& out, const std::wstring& wstr);
#endif // defined(Q_OS_WINDOWS)

QTextStream& operator<<(QTextStream& out, const char8_t* ustr);
QTextStream& operator<<(QTextStream& out, const std::u8string& ustr);

QTextStream& operator<<(QTextStream& out, const char16_t* ustr);
QTextStream& operator<<(QTextStream& out, const std::u16string& ustr);

QTextStream& operator<<(QTextStream& out, const std::filesystem::path& path);
QTextStream& operator<<(QTextStream& out, const std::error_code& error);

QTextStream& operator<<(QTextStream& out, const QStringList& qstrlist);

QTextStream& operator<<(QTextStream& out, const QPoint& qpoint);
QTextStream& operator<<(QTextStream& out, const QRect& qrect);
QTextStream& operator<<(QTextStream& out, const QSize& qsize);

QTextStream& operator<<(QTextStream& out, Qt::HANDLE handle);

// The NOTIMPLEMENTED() macro annotates codepaths which have not been implemented yet. If output
// spam is a serious concern, NOTIMPLEMENTED_LOG_ONCE can be used.
#define NOTIMPLEMENTED_MSG "NOT IMPLEMENTED"

#define NOTIMPLEMENTED() LOG(LS_ERROR) << NOTIMPLEMENTED_MSG
#define NOTIMPLEMENTED_LOG_ONCE()                                                                \
    do                                                                                           \
    {                                                                                            \
        static bool logged_once = false;                                                         \
        LOG_IF(LS_ERROR, !logged_once) << NOTIMPLEMENTED_MSG;                                    \
        logged_once = true;                                                                      \
    } while (0);                                                                                 \
    EAT_STREAM_PARAMETERS

#endif // BASE_LOGGING_H
