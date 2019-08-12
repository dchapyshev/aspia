//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__COMMAND_LINE_H
#define BASE__COMMAND_LINE_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace base {

class CommandLine
{
public:
    using StringVector = std::vector<std::u16string>;
    using SwitchMap = std::map<std::u16string, std::u16string, std::less<>>;

    // A constructor for CommandLines that only carry switches and arguments.
    enum NoProgram { NO_PROGRAM };
    explicit CommandLine(NoProgram no_program);

    // Construct a new command line with |program| as argv[0].
    explicit CommandLine(const std::filesystem::path& program);

    // Construct a new command line from an argument list.
    CommandLine(int argc, const char16_t* const* argv);
    CommandLine(int argc, const char* const* argv);
    explicit CommandLine(const StringVector& argv);

    CommandLine(const CommandLine& other) = default;
    CommandLine& operator=(const CommandLine& other) = default;

    CommandLine(CommandLine&& other);
    CommandLine& operator=(CommandLine&& other);

    ~CommandLine() = default;

    static CommandLine fromString(std::u16string_view command_line);
    static const CommandLine& forCurrentProcess();

    // Initialize from an argv vector.
    void initFromArgv(int argc, const char16_t* const* argv);
    void initFromArgv(int argc, const char* const* argv);
    void initFromArgv(const StringVector& argv);

    // Constructs and returns the represented command line string.
    std::u16string commandLineString() const
    {
        return commandLineStringInternal(false);
    }

    std::u16string argumentsString() const
    {
        return argumentsStringInternal(false);
    }

    // Get and Set the program part of the command line string (the first item).
    std::filesystem::path program() const;
    void setProgram(const std::filesystem::path& program);

    bool isEmpty() const;

    // Returns true if this command line contains the given switch.
    // Switch names must be lowercase.
    // The second override provides an optimized version to avoid inlining codegen at every
    // callsite to find the length of the constant and construct a string.
    bool hasSwitch(std::u16string_view switch_string) const;

    // Returns the value associated with the given switch. If the switch has no value or isn't
    // present, this method returns the empty string. Switch names must be lowercase.
    const std::filesystem::path& switchValuePath(std::u16string_view switch_string) const;
    const std::u16string& switchValue(std::u16string_view switch_string) const;

    void appendSwitch(std::u16string_view switch_string);
    void appendSwitchPath(std::u16string_view switch_string, const std::filesystem::path& path);
    void appendSwitch(std::u16string_view switch_string, std::u16string_view value);

    // Removes a switch.
    void removeSwitch(std::u16string_view switch_string);

    // Get the remaining arguments to the command.
    StringVector args() const;

    // Append an argument to the command line. Note that the argument is quoted properly such that
    // it is interpreted as one argument to the target command.
    void appendArgPath(const std::filesystem::path& value);
    void appendArg(std::u16string_view value);

    // Initialize by parsing the given command line string.
    // The program name is assumed to be the first item in the string.
    void parseFromString(std::u16string_view command_line);

private:
    // Disallow default constructor; a program name must be explicitly specified.
    CommandLine() = delete;

    // Internal version of commandLineString. If |quote_placeholders| is true,
    // also quotes parts with '%' in them.
    std::u16string commandLineStringInternal(bool quote_placeholders) const;

    // Internal version of argumentsString. If |quote_placeholders| is true, also quotes parts
    // with '%' in them.
    std::u16string argumentsStringInternal(bool quote_placeholders) const;

    // The argv array: { program, [(--|-|/)switch[=value]]*, [--], [argument]* }
    StringVector argv_;

    // Parsed-out switch keys and values.
    SwitchMap switches_;

    // The index after the program and switches, any arguments start here.
    size_t begin_args_;
};

} // namespace base

#endif // BASE__COMMAND_LINE_H
