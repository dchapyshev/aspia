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

namespace base {

#if defined(Q_OS_WINDOWS)

namespace {

constexpr int kMaxFrames = 128;
constexpr DWORD kMaxSymbolNameLength = 512;

struct FrameInfo
{
    DWORD64 address = 0;
    QString module;
    QString symbol;
    QString source;
};

QMutex g_dbghelp_lock;
std::atomic<int> g_crash_log_fd{-1};
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

// Requires g_dbghelp_lock to be held.
//--------------------------------------------------------------------------------------------------
FrameInfo extractFrameInfo(DWORD64 address)
{
    FrameInfo info;
    info.address = address;

    HANDLE process = GetCurrentProcess();

    char buffer[sizeof(SYMBOL_INFO) + kMaxSymbolNameLength] = { 0 };
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

    IMAGEHLP_LINE64 line = { 0 };
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD line_displacement = 0;
    if (SymGetLineFromAddr64(process, address, &line_displacement, &line) && line.FileName)
    {
        info.source = fileBasename(line.FileName);
        info.source += ':';
        info.source += QString::number(line.LineNumber);
    }

    IMAGEHLP_MODULE64 module_info = { 0 };
    module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (SymGetModuleInfo64(process, address, &module_info))
        info.module = QString::fromLocal8Bit(module_info.ModuleName);
    else
        info.module = "<unknown>";

    return info;
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

// Requires g_dbghelp_lock to be held.
//--------------------------------------------------------------------------------------------------
QList<FrameInfo> walkStackFromContext(const CONTEXT* context)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT local_context = *context;

    STACKFRAME64 frame = { 0 };
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
    const int fd = g_crash_log_fd.load(std::memory_order_relaxed);
    if (fd >= 0)
        _write(fd, data, static_cast<unsigned int>(size));

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

} // namespace

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
void setCrashLogFileDescriptor(int fd)
{
    g_crash_log_fd.store(fd, std::memory_order_relaxed);
}

//--------------------------------------------------------------------------------------------------
QString captureStackTrace(int skip_frames)
{
    void* addresses[kMaxFrames] = { 0 };
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

#else // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void installCrashHandler(const QString& /* dump_file_prefix */)
{
    // TODO: implement for Linux/macOS (sigaction + backtrace()).
}

//--------------------------------------------------------------------------------------------------
void setCrashLogFileDescriptor(int /* fd */)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
QString captureStackTrace(int /* skip_frames */)
{
    return QString();
}

#endif // defined(Q_OS_WINDOWS)

} // namespace base
