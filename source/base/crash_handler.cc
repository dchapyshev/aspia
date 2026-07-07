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

#include "base/crash_handler.h"

#include <QDateTime>
#include <QDir>
#include <QList>
#include <QMutex>

#include <algorithm>
#include <atomic>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <DbgHelp.h>
#include <io.h>
#endif // defined(Q_OS_WINDOWS)

#if (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)) || defined(Q_OS_MACOS)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#endif

namespace {

#if defined(Q_OS_WINDOWS) || (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)) || defined(Q_OS_MACOS)

constexpr int kMaxFrames = 128;

struct FrameInfo
{
    quint64 address = 0;
    QString module;
    QString symbol;
    QString source;
};

//--------------------------------------------------------------------------------------------------
QString fileBasename(const char* path)
{
    if (!path)
        return QString();

    const char* last = path;
    for (const char* p = path; *p; ++p)
    {
        if (*p == '\\' || *p == '/')
            last = p + 1;
    }
    return QString::fromLocal8Bit(last);
}

//--------------------------------------------------------------------------------------------------
QString formatStackTable(const QList<FrameInfo>& frames)
{
    if (frames.isEmpty())
        return QString();

    int idx_w = 1;
    int mod_w = 6; // "Module"
    int sym_w = 6; // "Symbol"
    int src_w = 6; // "Source"

    idx_w = std::max(idx_w, static_cast<int>(QString::number(frames.size() - 1).size()));

    for (const FrameInfo& f : frames)
    {
        mod_w = std::max(mod_w, static_cast<int>(f.module.size()));
        sym_w = std::max(sym_w, static_cast<int>(f.symbol.size()));
        src_w = std::max(src_w, static_cast<int>(f.source.size()));
    }

    constexpr int kAddrWidth = 18; // "0x" + 16 hex digits

    auto makeRow = [&](const QString& idx, const QString& addr,
                       const QString& mod, const QString& sym, const QString& src)
    {
        QString row = "  ";
        row += idx.leftJustified(idx_w, ' ');
        row += "  ";
        row += addr.leftJustified(kAddrWidth, ' ');
        row += "  ";
        row += mod.leftJustified(mod_w, ' ');
        row += "  ";
        row += sym.leftJustified(sym_w, ' ');
        row += "  ";
        row += src;
        row += '\n';
        return row;
    };

    QString result;
    result += makeRow("#", "Address", "Module", "Symbol", "Source");
    result += makeRow(QString(idx_w, '-'),
                      QString(kAddrWidth, '-'),
                      QString(mod_w, '-'),
                      QString(sym_w, '-'),
                      QString(src_w, '-'));

    for (int i = 0; i < frames.size(); ++i)
    {
        const FrameInfo& f = frames[i];
        QString addr = QString::asprintf("0x%016llX", static_cast<unsigned long long>(f.address));
        result += makeRow(QString::number(i), addr, f.module, f.symbol, f.source);
    }
    return result;
}

#endif

#if defined(Q_OS_WINDOWS)

constexpr DWORD kMaxSymbolNameLength = 512;

QMutex g_dbghelp_lock;
std::atomic<HANDLE> g_crash_log_handle{INVALID_HANDLE_VALUE};
std::atomic<bool> g_symbols_initialized{false};
QString g_dump_file_prefix;

//--------------------------------------------------------------------------------------------------
const char* exceptionCodeName(DWORD code)
{
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
        case 0xE06D7363:                         return "Microsoft C++ Exception";
        default:                                 return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
void initializeSymbols()
{
    bool expected = false;
    if (!g_symbols_initialized.compare_exchange_strong(expected, true))
        return;

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

// Requires g_dbghelp_lock to be held.
//--------------------------------------------------------------------------------------------------
FrameInfo extractFrameInfo(DWORD64 address)
{
    FrameInfo info;
    info.address = address;

    HANDLE process = GetCurrentProcess();

    char buffer[sizeof(SYMBOL_INFO) + kMaxSymbolNameLength] = {};
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kMaxSymbolNameLength;

    DWORD64 symbol_displacement = 0;
    if (SymFromAddr(process, address, &symbol_displacement, symbol))
    {
        info.symbol = QString::fromLocal8Bit(symbol->Name, static_cast<int>(symbol->NameLen));
        info.symbol += QString::asprintf("+0x%llX", static_cast<unsigned long long>(symbol_displacement));
    }
    else
    {
        info.symbol = "<unknown>";
    }

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD line_displacement = 0;
    if (SymGetLineFromAddr64(process, address, &line_displacement, &line) && line.FileName)
    {
        info.source = fileBasename(line.FileName);
        info.source += ':';
        info.source += QString::number(line.LineNumber);
    }

    IMAGEHLP_MODULE64 module_info = {};
    module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (SymGetModuleInfo64(process, address, &module_info))
        info.module = QString::fromLocal8Bit(module_info.ModuleName);
    else
        info.module = "<unknown>";

    return info;
}

// Requires g_dbghelp_lock to be held.
//--------------------------------------------------------------------------------------------------
QList<FrameInfo> walkStackFromContext(const CONTEXT* context)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT local_context = *context;

    STACKFRAME64 frame = {};
    DWORD machine_type = 0;

#if defined(_M_X64)
    machine_type = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = local_context.Rip;
    frame.AddrFrame.Offset = local_context.Rbp;
    frame.AddrStack.Offset = local_context.Rsp;
#elif defined(_M_IX86)
    machine_type = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = local_context.Eip;
    frame.AddrFrame.Offset = local_context.Ebp;
    frame.AddrStack.Offset = local_context.Esp;
#elif defined(_M_ARM64)
    machine_type = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset    = local_context.Pc;
    frame.AddrFrame.Offset = local_context.Fp;
    frame.AddrStack.Offset = local_context.Sp;
#else
#error Unsupported architecture
#endif

    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    QList<FrameInfo> result;

    for (int i = 0; i < kMaxFrames; ++i)
    {
        if (!StackWalk64(machine_type, process, thread, &frame, &local_context,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
        {
            break;
        }

        if (frame.AddrPC.Offset == 0)
            break;

        result.append(extractFrameInfo(frame.AddrPC.Offset));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void writeRawToCrashLog(const char* data, size_t size)
{
    HANDLE handle = g_crash_log_handle.load(std::memory_order_relaxed);
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
    {
        DWORD written = 0;
        WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr);
    }

    fwrite(data, 1, size, stderr);
}

//--------------------------------------------------------------------------------------------------
void writeRawToCrashLog(const QString& str)
{
    QByteArray utf8 = str.toUtf8();
    writeRawToCrashLog(utf8.constData(), static_cast<size_t>(utf8.size()));
}

//--------------------------------------------------------------------------------------------------
void writeMinidump(EXCEPTION_POINTERS* exception_pointers)
{
    const QString dir_path = QDir::tempPath() + "/aspia";
    QDir().mkpath(dir_path);

    const QString time_stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss.zzz");
    const QString file_path = QString("%1/%2-%3.dmp").arg(dir_path, g_dump_file_prefix, time_stamp);

    HANDLE dump_file = CreateFileW(qUtf16Printable(file_path), GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                   0, nullptr);

    if (dump_file == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exception_information;
    exception_information.ThreadId = GetCurrentThreadId();
    exception_information.ExceptionPointers = exception_pointers;
    exception_information.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, MiniDumpWithFullMemory,
                      &exception_information, nullptr, nullptr);
    CloseHandle(dump_file);
}

//--------------------------------------------------------------------------------------------------
QString formatCrashHeader(const EXCEPTION_RECORD* exception)
{
    const DWORD code = exception->ExceptionCode;
    const DWORD64 address = reinterpret_cast<DWORD64>(exception->ExceptionAddress);

    constexpr int kLabelWidth = 11;

    auto row = [&](const QString& label, const QString& value)
    {
        QString line = "  ";
        line += label.leftJustified(kLabelWidth, ' ');
        line += ": ";
        line += value;
        line += '\n';
        return line;
    };

    QString header = "=========================== CRASH REPORT ===========================\n";
    header += row("Thread", QString::number(GetCurrentThreadId()));
    header += row("Exception",
                  QString::asprintf("%s (0x%08lX)", exceptionCodeName(code), code));
    header += row("Address",
                  QString::asprintf("0x%016llX", static_cast<unsigned long long>(address)));

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)
    {
        if (exception->NumberParameters >= 2)
        {
            const ULONG_PTR op = exception->ExceptionInformation[0];
            const ULONG_PTR op_addr = exception->ExceptionInformation[1];
            const char* op_name = "unknown";
            switch (op)
            {
                case 0: op_name = "read";    break;
                case 1: op_name = "write";   break;
                case 8: op_name = "execute"; break;
                default: break;
            }
            header += row("Access",
                          QString::asprintf("%s at 0x%016llX",
                                            op_name, static_cast<unsigned long long>(op_addr)));
        }
    }

    header += '\n';
    return header;
}

//--------------------------------------------------------------------------------------------------
LONG WINAPI exceptionFilter(EXCEPTION_POINTERS* exception_pointers)
{
    static std::atomic<bool> in_handler{false};
    bool expected = false;
    if (!in_handler.compare_exchange_strong(expected, true))
        return EXCEPTION_EXECUTE_HANDLER;

    QString header = formatCrashHeader(exception_pointers->ExceptionRecord);

    QString table;
    {
        QMutexLocker lock(&g_dbghelp_lock);
        initializeSymbols();
        table = formatStackTable(walkStackFromContext(exception_pointers->ContextRecord));
    }

    writeRawToCrashLog(header);
    writeRawToCrashLog(table);
    writeMinidump(exception_pointers);

    return EXCEPTION_EXECUTE_HANDLER;
}

#elif (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)) || defined(Q_OS_MACOS)

// Fatal signals the crash handler is installed for.
const int kFatalSignals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGSYS };

std::atomic<int> g_crash_log_fd{-1};
std::atomic<bool> g_in_handler{false};

// The handler is delivered on this stack, so a stack-overflow SIGSEGV can still be reported.
char g_alt_stack[64 * 1024];

//--------------------------------------------------------------------------------------------------
const char* signalName(int sig)
{
    switch (sig)
    {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        case SIGSYS:  return "SIGSYS";
        default:      return "UNKNOWN";
    }
}

// Async-signal-safe formatting helpers: the signal handler cannot use QString or even snprintf.
//--------------------------------------------------------------------------------------------------
size_t appendString(char* buffer, size_t pos, size_t capacity, const char* str)
{
    while (*str && pos < capacity - 1)
        buffer[pos++] = *str++;
    buffer[pos] = 0;
    return pos;
}

//--------------------------------------------------------------------------------------------------
size_t appendDec(char* buffer, size_t pos, size_t capacity, unsigned long long value)
{
    char digits[24];
    int count = 0;

    do
    {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (value);

    while (count > 0 && pos < capacity - 1)
        buffer[pos++] = digits[--count];
    buffer[pos] = 0;
    return pos;
}

//--------------------------------------------------------------------------------------------------
size_t appendHex(char* buffer, size_t pos, size_t capacity, unsigned long long value)
{
    char hex[19] = "0x";
    for (int i = 0; i < 16; ++i)
        hex[2 + i] = "0123456789ABCDEF"[(value >> (60 - i * 4)) & 0xF];
    hex[18] = 0;
    return appendString(buffer, pos, capacity, hex);
}

//--------------------------------------------------------------------------------------------------
void writeRawToCrashLog(const char* data, size_t size)
{
    const int fd = g_crash_log_fd.load(std::memory_order_relaxed);
    if (fd != -1)
    {
        const ssize_t written = write(fd, data, size);
        (void)written;
    }

    const ssize_t written = write(STDERR_FILENO, data, size);
    (void)written;
}

//--------------------------------------------------------------------------------------------------
void signalHandler(int sig, siginfo_t* info, void* /* context */)
{
    // A crash inside the handler (or a concurrently crashing thread) goes straight to the default
    // disposition.
    bool expected = false;
    if (!g_in_handler.compare_exchange_strong(expected, true))
    {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }

    char buffer[512];
    size_t pos = 0;
    buffer[0] = 0;

    pos = appendString(buffer, pos, sizeof(buffer),
                       "=========================== CRASH REPORT ===========================\n");
    pos = appendString(buffer, pos, sizeof(buffer), "  Signal     : ");
    pos = appendString(buffer, pos, sizeof(buffer), signalName(sig));
    pos = appendString(buffer, pos, sizeof(buffer), " (");
    pos = appendDec(buffer, pos, sizeof(buffer), static_cast<unsigned long long>(sig));
    pos = appendString(buffer, pos, sizeof(buffer), ")\n  Code       : ");
    pos = appendDec(buffer, pos, sizeof(buffer), static_cast<unsigned long long>(info->si_code));
    pos = appendString(buffer, pos, sizeof(buffer), "\n  Address    : ");
    pos = appendHex(buffer, pos, sizeof(buffer), reinterpret_cast<unsigned long long>(info->si_addr));
    pos = appendString(buffer, pos, sizeof(buffer), "\n\n");

    writeRawToCrashLog(buffer, pos);

    // The raw return addresses; symbolization is left to the developer (addr2line/atos), doing it
    // here would not be async-signal-safe.
    void* frames[kMaxFrames];
    const int count = backtrace(frames, kMaxFrames);

    const int fd = g_crash_log_fd.load(std::memory_order_relaxed);
    if (fd != -1)
        backtrace_symbols_fd(frames, count, fd);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);

    // Re-raise with the default disposition so the OS produces a core dump (systemd-coredump on
    // Linux, ReportCrash on macOS) - the platform analogue of the Windows minidump.
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif

} // namespace

#if defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void installCrashHandler(const QString& dump_file_prefix)
{
    g_dump_file_prefix = dump_file_prefix;

    {
        QMutexLocker lock(&g_dbghelp_lock);
        initializeSymbols();
    }

    SetUnhandledExceptionFilter(exceptionFilter);

    // Reserve stack space for the handler so that a stack-overflow exception can still be reported.
    ULONG stack_size = 64 * 1024;
    SetThreadStackGuarantee(&stack_size);
}

//--------------------------------------------------------------------------------------------------
void setCrashLogHandle(qintptr handle)
{
    HANDLE h = (handle == -1) ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(handle);
    g_crash_log_handle.store(h, std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
QString captureStackTrace(int skip_frames)
{
    void* addresses[kMaxFrames] = {};
    const int skip = skip_frames < 0 ? 0 : skip_frames;
    const USHORT captured = CaptureStackBackTrace(
        static_cast<DWORD>(skip), kMaxFrames, addresses, nullptr);

    QMutexLocker lock(&g_dbghelp_lock);
    initializeSymbols();

    QList<FrameInfo> frames;
    frames.reserve(captured);
    for (USHORT i = 0; i < captured; ++i)
        frames.append(extractFrameInfo(reinterpret_cast<DWORD64>(addresses[i])));

    return formatStackTable(frames);
}

#elif (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)) || defined(Q_OS_MACOS)

//--------------------------------------------------------------------------------------------------
void installCrashHandler(const QString& /* dump_file_prefix */)
{
    // The prefix is not used: crash artifacts on these platforms are the OS core dumps, named by
    // the system.

    // The first backtrace() call loads the unwinder library, which allocates; do it now so the
    // call inside the signal handler is async-signal-safe.
    void* warmup[2];
    backtrace(warmup, 2);

    stack_t alt_stack = {};
    alt_stack.ss_sp = g_alt_stack;
    alt_stack.ss_size = sizeof(g_alt_stack);
    sigaltstack(&alt_stack, nullptr);

    struct sigaction action = {};
    action.sa_sigaction = signalHandler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    // Hold back the other fatal signals while the report is being written.
    for (int sig : kFatalSignals)
        sigaddset(&action.sa_mask, sig);

    for (int sig : kFatalSignals)
        sigaction(sig, &action, nullptr);
}

//--------------------------------------------------------------------------------------------------
void setCrashLogHandle(qintptr handle)
{
    g_crash_log_fd.store(static_cast<int>(handle), std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
QString captureStackTrace(int skip_frames)
{
    void* addresses[kMaxFrames] = {};
    const int captured = backtrace(addresses, kMaxFrames);
    const int skip = std::clamp(skip_frames, 0, captured);

    QList<FrameInfo> frames;
    frames.reserve(captured - skip);

    for (int i = skip; i < captured; ++i)
    {
        FrameInfo info;
        info.address = reinterpret_cast<quint64>(addresses[i]);

        Dl_info dl_info = {};
        if (dladdr(addresses[i], &dl_info) != 0)
        {
            info.module = fileBasename(dl_info.dli_fname);

            if (dl_info.dli_sname)
            {
                int status = -1;
                char* demangled = abi::__cxa_demangle(dl_info.dli_sname, nullptr, nullptr, &status);
                info.symbol = (status == 0 && demangled) ? demangled : dl_info.dli_sname;
                free(demangled);

                const quint64 offset =
                    info.address - reinterpret_cast<quint64>(dl_info.dli_saddr);
                info.symbol += QString::asprintf("+0x%llX", static_cast<unsigned long long>(offset));
            }
        }

        if (info.module.isEmpty())
            info.module = "<unknown>";
        if (info.symbol.isEmpty())
            info.symbol = "<unknown>";

        frames.append(info);
    }

    return formatStackTable(frames);
}

#else // Android and any other platform without a crash handler implementation.

//--------------------------------------------------------------------------------------------------
void installCrashHandler(const QString& /* dump_file_prefix */)
{
    // No-op: bionic has no backtrace() before API 33.
}

//--------------------------------------------------------------------------------------------------
void setCrashLogHandle(qintptr /* handle */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString captureStackTrace(int /* skip_frames */)
{
    return QString();
}

#endif
