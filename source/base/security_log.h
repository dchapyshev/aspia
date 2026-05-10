//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_SECURITY_LOG_H
#define BASE_SECURITY_LOG_H

#include <QDebug>
#include <QString>

// Instructions
// ------------
//
// The security log records host security-relevant events (peer connect/disconnect, authentication
// failures, other notable activity). Syntax mirrors the regular LOG() macros: pick an event type
// and stream the details:
//
//   SLOG(CONNECT)    << "user=" << user << " peer=" << peer;
//   SLOG(DISCONNECT) << "reason=" << reason;
//   SLOG(ERROR)      << "auth failed peer=" << peer;
//   SLOG(WARNING)    << "...";
//   SLOG(INFO)       << "...";
//
// Output goes to a single shared file in a system-wide, access-restricted directory:
//   Windows: %ProgramData%\aspia\security\YYYY-Www.log
//   Linux:   /var/log/aspia/security/YYYY-Www.log
//   macOS:   /Library/Logs/aspia/security/YYYY-Www.log
//
// The file is opened lazily on the first write and rotated weekly (the file name encodes the
// ISO 8601 year and week number). Files older than 90 days are removed when the directory is
// initialized. The hosting process must run as SYSTEM (Windows) or root (POSIX); otherwise
// SLOG() becomes a no-op.
//
// Each line is formatted as:
//   <ISO-timestamp> <EVENT> <user-supplied details>
//
// SecurityLogMessage uses a regular QDebug stream, so spaces between operands and quotes around
// strings are added automatically.

using SecurityEvent = int;

[[maybe_unused]] const SecurityEvent SEC_CONNECT    = 0;
[[maybe_unused]] const SecurityEvent SEC_DISCONNECT = 1;
[[maybe_unused]] const SecurityEvent SEC_ERROR      = 2;
[[maybe_unused]] const SecurityEvent SEC_WARNING    = 3;
[[maybe_unused]] const SecurityEvent SEC_INFO       = 4;

// Returns the directory that holds security log files. The directory is not created here; use it
// for read-only purposes (locating files, opening in a file manager).
QString securityLogDirectory();

// Per-record message object. Constructed by the SLOG() macro, formatted in the destructor and
// appended to the shared security log file.
class SecurityLogMessage
{
public:
    explicit SecurityLogMessage(SecurityEvent event);
    ~SecurityLogMessage();

    QDebug& stream() { return stream_; }

private:
    SecurityEvent event_;
    QString string_;
    QDebug stream_;

    Q_DISABLE_COPY_MOVE(SecurityLogMessage)
};

#define COMPACT_SLOG_EX_CONNECT(ClassName, ...)    ::ClassName(::SEC_CONNECT, ##__VA_ARGS__)
#define COMPACT_SLOG_EX_DISCONNECT(ClassName, ...) ::ClassName(::SEC_DISCONNECT, ##__VA_ARGS__)
#define COMPACT_SLOG_EX_ERROR(ClassName, ...)      ::ClassName(::SEC_ERROR, ##__VA_ARGS__)
#define COMPACT_SLOG_EX_WARNING(ClassName, ...)    ::ClassName(::SEC_WARNING, ##__VA_ARGS__)
#define COMPACT_SLOG_EX_INFO(ClassName, ...)       ::ClassName(::SEC_INFO, ##__VA_ARGS__)

#define COMPACT_SLOG_CONNECT    COMPACT_SLOG_EX_CONNECT(SecurityLogMessage)
#define COMPACT_SLOG_DISCONNECT COMPACT_SLOG_EX_DISCONNECT(SecurityLogMessage)
#define COMPACT_SLOG_ERROR      COMPACT_SLOG_EX_ERROR(SecurityLogMessage)
#define COMPACT_SLOG_WARNING    COMPACT_SLOG_EX_WARNING(SecurityLogMessage)
#define COMPACT_SLOG_INFO       COMPACT_SLOG_EX_INFO(SecurityLogMessage)

#if defined(Q_OS_WINDOWS)
// wingdi.h defines ERROR to be 0. When we call SLOG(ERROR), it gets substituted with 0, and it
// expands to COMPACT_SLOG_0. To allow us to keep using this syntax, we define this macro to do the
// same thing as COMPACT_SLOG_ERROR.
#define COMPACT_SLOG_EX_0(ClassName, ...) ::ClassName(::SEC_ERROR, ##__VA_ARGS__)
#define COMPACT_SLOG_0 COMPACT_SLOG_EX_0(SecurityLogMessage)
[[maybe_unused]] const SecurityEvent SEC_0 = SEC_ERROR;
#endif

// We use the preprocessor's merging operator, "##", so that, e.g., SLOG(CONNECT) becomes the token
// COMPACT_SLOG_CONNECT.
#define SLOG_STREAM(severity) COMPACT_SLOG_ ## severity.stream()

// Writes one line to the security log. The argument must be one of CONNECT, DISCONNECT, ERROR,
// WARNING, INFO.
#define SLOG(severity) SLOG_STREAM(severity)

#endif // BASE_SECURITY_LOG_H
