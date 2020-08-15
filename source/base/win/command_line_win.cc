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

#include "base/command_line.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <Windows.h>
#include <shellapi.h>
#endif // defined(OS_WIN)

namespace base {

namespace {

const char16_t kSwitchTerminator[] = u"--";
const char16_t kSwitchValueSeparator = u'=';

const char16_t* const kSwitchPrefixes[] = { u"--", u"-", u"/" };
const std::size_t kSwitchPrefixesCount = std::size(kSwitchPrefixes);

const std::u16string kEmptyString;
const std::filesystem::path kEmptyPath;

std::size_t switchPrefixLength(std::u16string_view string)
{
    for (std::size_t i = 0; i < kSwitchPrefixesCount; ++i)
    {
        const std::u16string_view prefix(kSwitchPrefixes[i]);

        if (string.compare(0, prefix.length(), prefix) == 0)
            return prefix.length();
    }

    return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool isSwitch(std::u16string_view string,
              std::u16string& switch_string,
              std::u16string& switch_value)
{
    switch_string.clear();
    switch_value.clear();

    const std::size_t prefix_length = switchPrefixLength(string);
    if (prefix_length == 0 || prefix_length == string.length())
        return false;

    const std::size_t equals_position = string.find(kSwitchValueSeparator);
    switch_string = string.substr(0, equals_position);

    if (equals_position != std::u16string::npos)
        switch_value = string.substr(equals_position + 1);

    return true;
}

// Append switches and arguments, keeping switches before arguments.
void appendSwitchesAndArguments(CommandLine* command_line, const CommandLine::StringVector& argv)
{
    bool parse_switches = true;

    for (std::size_t i = 1; i < argv.size(); ++i)
    {
        std::u16string arg;
        trimWhitespace(argv[i], TRIM_ALL, &arg);

        std::u16string switch_string;
        std::u16string switch_value;

        parse_switches &= (arg != kSwitchTerminator);

        if (parse_switches && isSwitch(arg, switch_string, switch_value))
            command_line->appendSwitch(switch_string, switch_value);
        else
            command_line->appendArg(arg);
    }
}

// Quote a string as necessary for CommandLineToArgvW compatiblity *on Windows*.
std::u16string quoteForCommandLineToArgvW(const std::u16string& arg, bool quote_placeholders)
{
    // We follow the quoting rules of CommandLineToArgvW.
    // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
    std::u16string quotable_chars(u" \\\"");
    // We may also be required to quote '%', which is commonly used in a command
    // line as a placeholder. (It may be substituted for a string with spaces.)
    if (quote_placeholders)
        quotable_chars.push_back(u'%');

    if (arg.find_first_of(quotable_chars) == std::u16string::npos)
    {
        // No quoting necessary.
        return arg;
    }

    std::u16string out;
    out.push_back(u'"');

    for (std::size_t i = 0; i < arg.size(); ++i)
    {
        if (arg[i] == u'\\')
        {
            // Find the extent of this run of backslashes.
            const std::size_t start = i;
            std::size_t end = start + 1;

            for (; end < arg.size() && arg[end] == u'\\'; ++end)
            {
                // Nothing
            }

            std::size_t backslash_count = end - start;

            // Backslashes are escapes only if the run is followed by a double quote.
            // Since we also will end the string with a double quote, we escape for
            // either a double quote or the end of the string.
            if (end == arg.size() || arg[end] == '"')
            {
                // To quote, we need to output 2x as many backslashes.
                backslash_count *= 2;
            }

            for (std::size_t j = 0; j < backslash_count; ++j)
                out.push_back(u'\\');

            // Advance i to one before the end to balance i++ in loop.
            i = end - 1;
        }
        else if (arg[i] == u'"')
        {
            out.push_back(u'\\');
            out.push_back(u'"');
        }
        else
        {
            out.push_back(arg[i]);
        }
    }

    out.push_back(u'"');
    return out;
}

} // namespace

CommandLine::CommandLine(NoProgram /* no_program */)
    : argv_(1),
      begin_args_(1)
{
    // Nothing
}

CommandLine::CommandLine(const std::filesystem::path& program)
    : argv_(1),
      begin_args_(1)
{
    setProgram(program);
}

CommandLine::CommandLine(int argc, const char16_t* const* argv)
    : argv_(1),
      begin_args_(1)
{
    initFromArgv(argc, argv);
}

CommandLine::CommandLine(int argc, const char* const* argv)
    : argv_(1),
      begin_args_(1)
{
    initFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1)
{
    initFromArgv(argv);
}

CommandLine::CommandLine(CommandLine&& other) noexcept
    : argv_(std::move(other.argv_)),
      switches_(std::move(other.switches_)),
      begin_args_(other.begin_args_)
{
    // Nothing
}

CommandLine& CommandLine::operator=(CommandLine&& other) noexcept
{
    if (this != &other)
    {
        argv_ = std::move(other.argv_);
        switches_ = std::move(other.switches_);
        begin_args_ = other.begin_args_;
    }

    return *this;
}

// static
CommandLine CommandLine::fromString(std::u16string_view command_line)
{
    CommandLine cmd(NO_PROGRAM);
    cmd.parseFromString(command_line);
    return cmd;
}

// static
const CommandLine& CommandLine::forCurrentProcess()
{
    static CommandLine command_line =
        CommandLine::fromString(asWritableUtf16(GetCommandLineW()));

    return command_line;
}

void CommandLine::initFromArgv(int argc, const char16_t* const* argv)
{
    StringVector new_argv;

    for (int i = 0; i < argc; ++i)
        new_argv.emplace_back(argv[i]);

    initFromArgv(new_argv);
}

void CommandLine::initFromArgv(int argc, const char* const* argv)
{
    StringVector new_argv;

    for (int i = 0; i < argc; ++i)
        new_argv.emplace_back(utf16FromLocal8Bit(argv[i]));

    initFromArgv(new_argv);
}

void CommandLine::initFromArgv(const StringVector& argv)
{
    argv_ = StringVector(1);
    switches_.clear();
    begin_args_ = 1;
    setProgram(argv.empty() ? std::filesystem::path() : argv[0]);
    appendSwitchesAndArguments(this, argv);
}

std::filesystem::path CommandLine::program() const
{
    return argv_[0];
}

void CommandLine::setProgram(const std::filesystem::path& program)
{
    trimWhitespace(program.u16string(), TRIM_ALL, &argv_[0]);
}

bool CommandLine::isEmpty() const
{
    return argv_.size() == 1 && switches_.empty();
}

bool CommandLine::hasSwitch(std::u16string_view switch_string) const
{
    DCHECK(toLower(switch_string) == switch_string);
    return switches_.find(switch_string) != switches_.end();
}

const std::filesystem::path& CommandLine::switchValuePath(std::u16string_view switch_string) const
{
    DCHECK(toLower(switch_string) == switch_string);
    auto result = switches_.find(switch_string);
    return result == switches_.end() ? kEmptyPath : result->second;
}

const std::u16string& CommandLine::switchValue(std::u16string_view switch_string) const
{
    DCHECK(toLower(switch_string) == switch_string);
    auto result = switches_.find(switch_string);
    return result == switches_.end() ? kEmptyString : result->second;
}

void CommandLine::appendSwitch(std::u16string_view switch_string)
{
    appendSwitch(switch_string, std::u16string());
}

void CommandLine::appendSwitchPath(std::u16string_view switch_string,
                                   const std::filesystem::path& path)
{
    appendSwitch(switch_string, path.u16string());
}

void CommandLine::appendSwitch(std::u16string_view switch_string, std::u16string_view value)
{
    const std::u16string switch_key = toLower(switch_string);
    std::u16string combined_switch_string(switch_key);

    const std::size_t prefix_length = switchPrefixLength(combined_switch_string);
    auto insertion = switches_.insert(make_pair(switch_key.substr(prefix_length), value));

    if (!insertion.second)
        insertion.first->second = value;

    // Preserve existing switch prefixes in |argv_|; only append one if necessary.
    if (prefix_length == 0)
        combined_switch_string = kSwitchPrefixes[0] + combined_switch_string;

    if (!value.empty())
    {
        combined_switch_string += kSwitchValueSeparator;
        combined_switch_string += value;
    }

    // Append the switch and update the switches/arguments divider |begin_args_|.
    argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

void CommandLine::removeSwitch(std::u16string_view switch_string)
{
    DCHECK(toLowerASCII(switch_string) == switch_string);
    switches_.erase(std::u16string(switch_string));
}

CommandLine::StringVector CommandLine::args() const
{
    // Gather all arguments after the last switch (may include kSwitchTerminator).
    StringVector args(argv_.begin() + begin_args_, argv_.end());

    // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
    StringVector::iterator switch_terminator =
        std::find(args.begin(), args.end(), kSwitchTerminator);

    if (switch_terminator != args.end())
        args.erase(switch_terminator);

    return args;
}

void CommandLine::appendArgPath(const std::filesystem::path& value)
{
    appendArg(value.u16string());
}

void CommandLine::appendArg(std::u16string_view value)
{
    argv_.emplace_back(value);
}

void CommandLine::parseFromString(std::u16string_view command_line)
{
    command_line = trimWhitespace(command_line, TRIM_ALL);
    if (command_line.empty())
        return;

    int num_args = 0;
    wchar_t** args = nullptr;

    // When calling CommandLineToArgvW, use the apiset if available.
    // Doing so will bypass loading shell32.dll on Win8+.
    HMODULE downlevel_shell32_dll =
        LoadLibraryExW(L"api-ms-win-downlevel-shell32-l1-1-0.dll",
                       nullptr,
                       LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (downlevel_shell32_dll)
    {
        auto command_line_to_argv_w_proc =
            reinterpret_cast<decltype(CommandLineToArgvW)*>(
                GetProcAddress(downlevel_shell32_dll, "CommandLineToArgvW"));
        if (command_line_to_argv_w_proc)
        {
            args = command_line_to_argv_w_proc(asWide(command_line), &num_args);
        }

        FreeLibrary(downlevel_shell32_dll);
    }
    else
    {
        // Since the apiset is not available, allow the delayload of shell32.dll to take place.
        args = CommandLineToArgvW(asWide(command_line), &num_args);
    }

    auto val = ::base::LS_FATAL;
    DLOG_IF(LS_FATAL, !args) << "CommandLineToArgvW failed on command line: "
                             << command_line.data();

    initFromArgv(num_args, reinterpret_cast<const char16_t* const*>(args));
    LocalFree(args);
}

std::u16string CommandLine::commandLineStringInternal(bool quote_placeholders) const
{
    std::u16string string = quoteForCommandLineToArgvW(argv_[0], quote_placeholders);
    const std::u16string params(argumentsStringInternal(quote_placeholders));

    if (!params.empty())
    {
        string.append(u" ");
        string.append(params);
    }

    return string;
}

std::u16string CommandLine::argumentsStringInternal(bool quote_placeholders) const
{
    std::u16string params;

    // Append switches and arguments.
    bool parse_switches = true;

    for (std::size_t i = 1; i < argv_.size(); ++i)
    {
        std::u16string arg = argv_[i];
        std::u16string switch_string;
        std::u16string switch_value;

        parse_switches &= arg != kSwitchTerminator;

        if (i > 1)
            params.append(u" ");

        if (parse_switches && isSwitch(arg, switch_string, switch_value))
        {
            params.append(switch_string);

            if (!switch_value.empty())
            {
                switch_value = quoteForCommandLineToArgvW(switch_value, quote_placeholders);
                params.append(kSwitchValueSeparator + switch_value);
            }
        }
        else
        {
            arg = quoteForCommandLineToArgvW(arg, quote_placeholders);
            params.append(arg);
        }
    }

    return params;
}

} // namespace base
