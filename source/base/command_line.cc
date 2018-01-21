//
// PROJECT:         Aspia
// FILE:            base/command_line.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_util.h"
#include "base/command_line.h"
#include "base/logging.h"

#include <shellapi.h>

namespace aspia {

namespace {

constexpr CommandLine::CharType kSwitchTerminator[] = L"--";
constexpr CommandLine::CharType kSwitchValueSeparator[] = L"=";
constexpr CommandLine::CharType* const kSwitchPrefix = L"--";

CommandLine current_process_command_line = CommandLine::FromString(GetCommandLineW());

size_t GetSwitchPrefixLength(const CommandLine::StringType& string)
{
    CommandLine::StringType prefix(kSwitchPrefix);

    if (string.compare(0, prefix.length(), prefix) == 0)
        return prefix.length();

    return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType& switch_string,
              CommandLine::StringType& switch_value)
{
    switch_string.clear();
    switch_value.clear();

    size_t prefix_length = GetSwitchPrefixLength(string);
    if (prefix_length == 0 || prefix_length == string.length())
        return false;

    const size_t equals_position = string.find(kSwitchValueSeparator);
    switch_string = string.substr(0, equals_position);

    if (equals_position != CommandLine::StringType::npos)
        switch_value = string.substr(equals_position + 1);

    return true;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line, const CommandLine::StringVector& argv)
{
    bool parse_switches = true;

    for (size_t i = 1; i < argv.size(); ++i)
    {
        CommandLine::StringType arg = argv[i];

        TrimWhitespace(arg, TRIM_ALL, arg);

        CommandLine::StringType switch_string;
        CommandLine::StringType switch_value;

        parse_switches &= (arg != kSwitchTerminator);

        if (parse_switches && IsSwitch(arg, switch_string, switch_value))
        {
            command_line->AppendSwitch(switch_string, switch_value);
        }
        else
        {
            command_line->AppendArg(arg);
        }
    }
}

// Quote a string as necessary for CommandLineToArgvW compatiblity *on Windows*.
CommandLine::StringType QuoteForCommandLineToArgvW(const CommandLine::StringType& arg,
                                                   bool quote_placeholders)
{
    // We follow the quoting rules of CommandLineToArgvW.
    // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
    CommandLine::StringType quotable_chars(L" \\\"");
    // We may also be required to quote '%', which is commonly used in a command
    // line as a placeholder. (It may be substituted for a string with spaces.)
    if (quote_placeholders)
        quotable_chars.push_back(L'%');

    if (arg.find_first_of(quotable_chars) == CommandLine::StringType::npos)
    {
        // No quoting necessary.
        return arg;
    }

    CommandLine::StringType out;
    out.push_back(L'"');

    for (size_t i = 0; i < arg.size(); ++i)
    {
        if (arg[i] == '\\')
        {
            // Find the extent of this run of backslashes.
            size_t start = i, end = start + 1;
            for (; end < arg.size() && arg[end] == '\\'; ++end) {}
            size_t backslash_count = end - start;

            // Backslashes are escapes only if the run is followed by a double quote.
            // Since we also will end the string with a double quote, we escape for
            // either a double quote or the end of the string.
            if (end == arg.size() || arg[end] == '"')
            {
                // To quote, we need to output 2x as many backslashes.
                backslash_count *= 2;
            }

            for (size_t j = 0; j < backslash_count; ++j)
                out.push_back('\\');

            // Advance i to one before the end to balance i++ in loop.
            i = end - 1;
        }
        else if (arg[i] == '"')
        {
            out.push_back('\\');
            out.push_back('"');
        }
        else
        {
            out.push_back(arg[i]);
        }
    }

    out.push_back('"');
    return out;
}

} // namespace

CommandLine::CommandLine(NoProgram /* no_program */)
    : argv_(1),
      begin_args_(1)
{
    // Nothing
}

CommandLine::CommandLine(const std::experimental::filesystem::path& program)
    : argv_(1),
      begin_args_(1)
{
    SetProgram(program);
}

CommandLine::CommandLine(int argc, const CharType* const* argv)
    : argv_(1),
      begin_args_(1)
{
    InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1)
{
    InitFromArgv(argv);
}

CommandLine::CommandLine(CommandLine&& other)
    : argv_(std::move(other.argv_)),
      switches_(std::move(other.switches_)),
      begin_args_(other.begin_args_)
{
    // Nothing
}

CommandLine& CommandLine::operator=(CommandLine&& other)
{
    argv_ = std::move(other.argv_);
    switches_ = std::move(other.switches_);
    begin_args_ = other.begin_args_;
    return *this;
}

// static
CommandLine CommandLine::FromString(const StringType& command_line)
{
    CommandLine cmd(NO_PROGRAM);
    cmd.ParseFromString(command_line);
    return cmd;
}

// static
const CommandLine& CommandLine::ForCurrentProcess()
{
    return current_process_command_line;
}

void CommandLine::InitFromArgv(int argc, const CommandLine::CharType* const* argv)
{
    StringVector new_argv;

    for (int i = 0; i < argc; ++i)
        new_argv.push_back(argv[i]);

    InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv)
{
    argv_ = StringVector(1);
    switches_.clear();
    begin_args_ = 1;
    SetProgram(argv.empty() ? std::experimental::filesystem::path() : argv[0]);
    AppendSwitchesAndArguments(this, argv);
}

std::experimental::filesystem::path CommandLine::GetProgram() const
{
    return argv_[0];
}

void CommandLine::SetProgram(const std::experimental::filesystem::path& program)
{
    TrimWhitespace(program, TRIM_ALL, argv_[0]);
}

bool CommandLine::IsEmpty() const
{
    return argv_.size() == 1 && switches_.empty();
}

bool CommandLine::HasSwitch(const StringType& switch_string) const
{
    DCHECK(ToLower(switch_string) == switch_string);
    return switches_.find(switch_string) != switches_.end();
}

bool CommandLine::HasSwitch(const CharType switch_constant[]) const
{
    return HasSwitch(StringType(switch_constant));
}

std::experimental::filesystem::path CommandLine::GetSwitchValuePath(
    const StringType& switch_string) const
{
    return std::experimental::filesystem::path(GetSwitchValue(switch_string));
}

CommandLine::StringType CommandLine::GetSwitchValue(const StringType& switch_string) const
{
    DCHECK(ToLower(switch_string) == switch_string);
    auto result = switches_.find(switch_string);
    return result == switches_.end() ? StringType() : result->second;
}

void CommandLine::AppendSwitch(const StringType& switch_string)
{
    AppendSwitch(switch_string, StringType());
}

void CommandLine::AppendSwitchPath(const StringType& switch_string,
                                   const std::experimental::filesystem::path& path)
{
    AppendSwitch(switch_string, path.native());
}

void CommandLine::AppendSwitch(const StringType& switch_string, const StringType& value)
{
    const StringType switch_key = ToLower(switch_string);
    StringType combined_switch_string(switch_key);

    size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
    auto insertion = switches_.insert(make_pair(switch_key.substr(prefix_length), value));

    if (!insertion.second)
        insertion.first->second = value;

    // Preserve existing switch prefixes in |argv_|; only append one if necessary.
    if (prefix_length == 0)
        combined_switch_string = kSwitchPrefix + combined_switch_string;

    if (!value.empty())
        combined_switch_string += kSwitchValueSeparator + value;

    // Append the switch and update the switches/arguments divider |begin_args_|.
    argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

void CommandLine::AppendArgPath(const std::experimental::filesystem::path& value)
{
    AppendArg(value.native());
}

void CommandLine::AppendArg(const StringType& value)
{
    argv_.push_back(value);
}

void CommandLine::ParseFromString(const StringType& command_line)
{
    StringType command_line_string;

    TrimWhitespace(command_line, TRIM_ALL, command_line_string);
    if (command_line_string.empty())
        return;

    int num_args = 0;
    wchar_t** args = nullptr;

    args = ::CommandLineToArgvW(command_line_string.c_str(), &num_args);
    DLOG_IF(FATAL, !args) << "CommandLineToArgvW failed on command line: " << command_line;

    InitFromArgv(num_args, args);
    LocalFree(args);
}

CommandLine::StringType CommandLine::GetCommandLineStringInternal(bool quote_placeholders) const
{
    StringType string(argv_[0]);
    string = QuoteForCommandLineToArgvW(string, quote_placeholders);
    StringType params(GetArgumentsStringInternal(quote_placeholders));

    if (!params.empty())
    {
        string.append(StringType(L" "));
        string.append(params);
    }

    return string;
}

CommandLine::StringType CommandLine::GetArgumentsStringInternal(bool quote_placeholders) const
{
    StringType params;

    // Append switches and arguments.
    bool parse_switches = true;

    for (size_t i = 1; i < argv_.size(); ++i)
    {
        StringType arg = argv_[i];
        StringType switch_string;
        StringType switch_value;

        parse_switches &= arg != kSwitchTerminator;

        if (i > 1)
            params.append(StringType(L" "));

        if (parse_switches && IsSwitch(arg, switch_string, switch_value))
        {
            params.append(switch_string);

            if (!switch_value.empty())
            {
                switch_value = QuoteForCommandLineToArgvW(switch_value, quote_placeholders);
                params.append(kSwitchValueSeparator + switch_value);
            }
        }
        else
        {
            arg = QuoteForCommandLineToArgvW(arg, quote_placeholders);
            params.append(arg);
        }
    }

    return params;
}

} // namespace aspia
