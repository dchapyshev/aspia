//
// PROJECT:         Aspia
// FILE:            base/command_line.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__COMMAND_LINE_H
#define _ASPIA_BASE__COMMAND_LINE_H

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>

namespace aspia {

class CommandLine
{
public:
    using StringType = std::wstring;
    using CharType = StringType::value_type;
    using StringVector = std::vector<StringType>;
    using SwitchMap = std::map<StringType, StringType, std::less<>>;

    // A constructor for CommandLines that only carry switches and arguments.
    enum NoProgram { NO_PROGRAM };
    explicit CommandLine(NoProgram no_program);

    // Construct a new command line with |program| as argv[0].
    explicit CommandLine(const std::experimental::filesystem::path& program);

    // Construct a new command line from an argument list.
    CommandLine(int argc, const CharType* const* argv);
    explicit CommandLine(const StringVector& argv);

    CommandLine(const CommandLine& other) = default;
    CommandLine& operator=(const CommandLine& other) = default;

    CommandLine(CommandLine&& other);
    CommandLine& operator=(CommandLine&& other);

    ~CommandLine() = default;

    static CommandLine FromString(const StringType& command_line);
    static const CommandLine& ForCurrentProcess();

    // Initialize from an argv vector.
    void InitFromArgv(int argc, const CharType* const* argv);
    void InitFromArgv(const StringVector& argv);

    // Constructs and returns the represented command line string.
    StringType GetCommandLineString() const
    {
        return GetCommandLineStringInternal(false);
    }

    // Get and Set the program part of the command line string (the first item).
    std::experimental::filesystem::path GetProgram() const;
    void SetProgram(const std::experimental::filesystem::path& program);

    bool IsEmpty() const;

    // Returns true if this command line contains the given switch.
    // Switch names must be lowercase.
    // The second override provides an optimized version to avoid inlining codegen
    // at every callsite to find the length of the constant and construct a
    // StringType.
    bool HasSwitch(const StringType& switch_string) const;
    bool HasSwitch(const CharType switch_constant[]) const;

    // Returns the value associated with the given switch. If the switch has no
    // value or isn't present, this method returns the empty string.
    // Switch names must be lowercase.
    std::experimental::filesystem::path GetSwitchValuePath(const StringType& switch_string) const;
    StringType GetSwitchValue(const StringType& switch_string) const;

    void AppendSwitch(const StringType& switch_string);
    void AppendSwitchPath(const StringType& switch_string,
                          const std::experimental::filesystem::path& path);
    void AppendSwitch(const StringType& switch_string, const StringType& value);

    // Append an argument to the command line. Note that the argument is quoted
    // properly such that it is interpreted as one argument to the target command.
    void AppendArgPath(const std::experimental::filesystem::path& value);
    void AppendArg(const StringType& value);

    // Initialize by parsing the given command line string.
    // The program name is assumed to be the first item in the string.
    void ParseFromString(const StringType& command_line);

private:
    // Disallow default constructor; a program name must be explicitly specified.
    CommandLine() = delete;

    // Internal version of GetCommandLineString. If |quote_placeholders| is true,
    // also quotes parts with '%' in them.
    StringType GetCommandLineStringInternal(bool quote_placeholders) const;

    // Internal version of GetArgumentsString. If |quote_placeholders| is true,
    // also quotes parts with '%' in them.
    StringType GetArgumentsStringInternal(bool quote_placeholders) const;

    // The argv array: { program, [(--|-|/)switch[=value]]*, [--], [argument]* }
    StringVector argv_;

    // Parsed-out switch keys and values.
    SwitchMap switches_;

    // The index after the program and switches, any arguments start here.
    size_t begin_args_;
};

} // namespace aspia

#endif // _ASPIA_BASE__COMMAND_LINE_H
