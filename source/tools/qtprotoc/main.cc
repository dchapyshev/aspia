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

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/printer.h>

#include <fstream>

//--------------------------------------------------------------------------------------------------
std::string qualifiedCppTypeName(const std::string& full_name)
{
    std::string result;

    for (char c : full_name)
        result += (c == '.') ? "::" : std::string(1, c);

    return result;
}

//--------------------------------------------------------------------------------------------------
void collectAllMessages(const google::protobuf::Descriptor* descriptor,
                        std::vector<const google::protobuf::Descriptor*>* messages)
{
    messages->emplace_back(descriptor);
    for (int i = 0; i < descriptor->nested_type_count(); ++i)
        collectAllMessages(descriptor->nested_type(i), messages);
}

//--------------------------------------------------------------------------------------------------
void collectFileMessages(const google::protobuf::FileDescriptor* file,
                         std::vector<const google::protobuf::Descriptor*>* messages)
{
    for (int i = 0; i < file->message_type_count(); ++i)
        collectAllMessages(file->message_type(i), messages);
}

//--------------------------------------------------------------------------------------------------
void collectFileEnums(const google::protobuf::FileDescriptor* file,
                      std::vector<const google::protobuf::EnumDescriptor*>* enums)
{
    for (int i = 0; i < file->enum_type_count(); ++i)
        enums->emplace_back(file->enum_type(i));

    for (int i = 0; i < file->message_type_count(); ++i)
    {
        const google::protobuf::Descriptor* msg = file->message_type(i);

        for (int j = 0; j < msg->enum_type_count(); ++j)
            enums->emplace_back(msg->enum_type(j));
    }
}

//--------------------------------------------------------------------------------------------------
std::string makeIncludeGuardMacro(const std::string& file_name)
{
    std::string guard = file_name;
    std::replace(guard.begin(), guard.end(), '.', '_');
    std::replace(guard.begin(), guard.end(), '/', '_');
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    return "PROTO_" + guard;
}

//--------------------------------------------------------------------------------------------------
std::string makeGetterName(const std::string& name)
{
    std::string getter = name;
    std::transform(getter.begin(), getter.end(), getter.begin(), ::tolower);
    return getter;
}

//--------------------------------------------------------------------------------------------------
void generateEnumOperator(
    const google::protobuf::EnumDescriptor* descriptor, google::protobuf::io::Printer& source)
{
    const std::string cpp_enum_type = qualifiedCppTypeName(descriptor->full_name());

    source.Print("QDebug operator<<(QDebug out, $TYPE$ value)\n", "TYPE", cpp_enum_type);
    source.Print("{\n");
    source.Print("    switch (value)\n"
                 "    {\n");

    for (int i = 0; i < descriptor->value_count(); ++i)
    {
        const auto* val = descriptor->value(i);
        std::string full_enum_scope = qualifiedCppTypeName(
            descriptor->containing_type() ? descriptor->containing_type()->full_name() : descriptor->full_name());
        std::string full_enum_const = full_enum_scope + "::" + val->name();

        source.Print("        case $CONST$:\n", "CONST", full_enum_const);
        source.Print("            return out << \"$CONST$\";\n", "CONST", val->name());
    }

    source.Print("        default:\n");
    source.Print("            return out << \"$TYPE$(UNKNOWN)\";\n", "TYPE", cpp_enum_type);
    source.Print("    }\n");
    source.Print("}\n");
    source.Print("\n");
}

//--------------------------------------------------------------------------------------------------
void generateMessageOperator(
    const google::protobuf::Descriptor* descriptor, google::protobuf::io::Printer& source)
{
    std::string cpp_type = qualifiedCppTypeName(descriptor->full_name());

    source.Print("QDebug operator<<(QDebug out, const $TYPE$& msg)\n{\n", "TYPE", cpp_type);
    source.Print("    out << \"$NAME$(\";\n", "NAME", descriptor->name());

    for (int i = 0; i < descriptor->field_count(); ++i)
    {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        std::string field_name = field->name();
        std::string getter_name = makeGetterName(field->name());

        if (field->is_repeated())
        {
            source.Print("    for (int i = 0; i < msg.$FIELD$_size(); ++i)\n", "FIELD", field_name);

            if (field->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
            {
                source.Print("        out << \"$FIELD$#\" << i << ':' << QString::fromStdString(msg.$GETTER$(i));\n",
                             "FIELD", field_name, "GETTER", getter_name);
            }
            else if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
            {
                source.Print("        out << \"$FIELD$#\" << i << ':' << QString::fromLatin1(QByteArray::fromStdString(msg.$GETTER$(i)).toHex());\n",
                             "FIELD", field_name, "GETTER", getter_name);
            }
            else
            {
                source.Print("        out << \"$FIELD$#\" << i << ':' << msg.$GETTER$(i);\n",
                             "FIELD", field_name, "GETTER", getter_name);
            }
        }
        else if (field->type() == google::protobuf::FieldDescriptor::TYPE_STRING)
        {
            source.Print("    out << \"$FIELD$:\" << QString::fromStdString(msg.$GETTER$());\n",
                         "FIELD", field_name, "GETTER", getter_name);
        }
        else if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES)
        {
            source.Print("    out << \"$FIELD$:\" << QString::fromLatin1(QByteArray::fromStdString(msg.$GETTER$()).toHex());\n",
                         "FIELD", field_name, "GETTER", getter_name);
        }
        else if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE)
        {
            source.Print("    if (msg.has_$FIELD$())\n"
                         "        out << \"$FIELD$:\" << msg.$GETTER$();\n",
                         "FIELD", field_name, "GETTER", getter_name);
        }
        else
        {
            source.Print("    out << \"$FIELD$:\" << msg.$GETTER$();\n",
                         "FIELD", field_name, "GETTER", getter_name);
        }
    }
    source.Print("    out << ')';\n");
    source.Print("    return out;\n");
    source.Print("}\n\n");
}

//--------------------------------------------------------------------------------------------------
void generateHeadComment(google::protobuf::io::Printer& file)
{
    file.Print("//\n");
    file.Print("// This file is generated. Do not modify it.\n");
    file.Print("//\n\n");
}

//--------------------------------------------------------------------------------------------------
void trimTrailingNuls(std::string& str)
{
    while (!str.empty() && str.back() == '\0')
    {
        str.pop_back();
    }
}

//--------------------------------------------------------------------------------------------------
void writeFileIfChanged(const std::string& filename,
                        const std::string& content,
                        google::protobuf::compiler::GeneratorContext* context)
{
    {
        std::ifstream existing_file(filename, std::ios::in | std::ios::binary);
        if (existing_file)
        {
            std::ostringstream ss;
            ss << existing_file.rdbuf();
            if (ss.str() == content)
                return;
        }
    }

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    out.write(content.data(), content.size());
}

//--------------------------------------------------------------------------------------------------
class MetatypeGenerator : public google::protobuf::compiler::CodeGenerator
{
public:
    bool Generate(const google::protobuf::FileDescriptor* file,
                  const std::string& parameter,
                  google::protobuf::compiler::GeneratorContext* context,
                  std::string* error) const override;
};

//--------------------------------------------------------------------------------------------------
bool MetatypeGenerator::Generate(const google::protobuf::FileDescriptor* file,
                                 const std::string&,
                                 google::protobuf::compiler::GeneratorContext* context,
                                 std::string* error) const
{
    std::string header_name = file->name();
    header_name.replace(header_name.rfind(".proto"), 6, ".h");

    std::string source_name = file->name();
    source_name.replace(source_name.rfind(".proto"), 6, ".cc");

    std::string pb_header = file->name();
    pb_header.replace(pb_header.rfind(".proto"), 6, ".pb.h");

    std::vector<const google::protobuf::Descriptor*> messages;
    collectFileMessages(file, &messages);

    std::vector<const google::protobuf::EnumDescriptor*> enums;
    collectFileEnums(file, &enums);

    //
    // Header File
    //

    std::string header_content;
    google::protobuf::io::StringOutputStream header_string_stream(&header_content);
    google::protobuf::io::Printer header(&header_string_stream, '$');

    const std::string include_guard = makeIncludeGuardMacro(header_name);

    generateHeadComment(header);

    header.Print("#ifndef $GUARD$\n#define $GUARD$\n", "GUARD", include_guard);
    header.Print("\n");
    header.Print("#include <QDebug>\n"
                 "#include <QMetaType>\n"
                 "\n");
    header.Print("#include \"proto/$PB_HEADER$\" // IWYU pragma: export\n", "PB_HEADER", pb_header);

    // Add includes for imported files
    for (int i = 0; i < file->dependency_count(); ++i)
    {
        std::string dep = file->dependency(i)->name();
        dep.replace(dep.rfind(".proto"), 6, ".h");
        header.Print("#include \"proto/$DEP$\" // IWYU pragma: export\n", "DEP", dep);
    }
    header.Print("\n");

    header.Print("// Meta type declarations for enumerations.\n");
    for (const auto* e : enums)
        header.Print("Q_DECLARE_METATYPE($TYPE$)\n", "TYPE", qualifiedCppTypeName(e->full_name()));

    header.Print("\n");

    header.Print("// Meta type declarations for messages.\n");
    for (const auto* msg : messages)
        header.Print("Q_DECLARE_METATYPE($TYPE$)\n", "TYPE", qualifiedCppTypeName(msg->full_name()));

    header.Print("\n");

    header.Print("// Stream operators for enumerations.\n");
    for (const auto* e : enums)
    {
        const std::string cpp_enum_type = qualifiedCppTypeName(e->full_name());
        header.Print("QDebug operator<<(QDebug out, $TYPE$ value);\n", "TYPE", cpp_enum_type);
    }

    header.Print("\n");

    header.Print("// Stream operators for messages.\n");
    for (const auto* msg : messages)
    {
        const std::string cpp_enum_type = qualifiedCppTypeName(msg->full_name());
        header.Print("QDebug operator<<(QDebug out, const $TYPE$& value);\n", "TYPE", cpp_enum_type);
    }

    header.Print("\n#endif // $GUARD$\n", "GUARD", include_guard);

    trimTrailingNuls(header_content);
    writeFileIfChanged(header_name, header_content, context);

    //
    // Source File
    //

    std::string source_content;
    google::protobuf::io::StringOutputStream source_string_stream(&source_content);
    google::protobuf::io::Printer source(&source_string_stream, '$');

    generateHeadComment(source);

    source.Print("#include \"proto/$HEADER$\"\n\n", "HEADER", header_name);

    source.Print("namespace {\n"
                 "\n"
                 "void registerProtobufTypes()\n"
                 "{\n");

    source.Print("    // Register enumerations.\n");
    for (const auto* e : enums)
    {
        source.Print("    qRegisterMetaType<$TYPE$>(\"$TYPE$\");\n",
                     "TYPE", qualifiedCppTypeName(e->full_name()));
    }

    source.Print("\n");

    source.Print("    // Register messages.\n");
    for (const auto* msg : messages)
    {
        source.Print("    qRegisterMetaType<$TYPE$>(\"$TYPE$\");\n",
                     "TYPE", qualifiedCppTypeName(msg->full_name()));
    }

    source.Print("}\n\n"
                 "struct Registrator\n"
                 "{\n"
                 "    Registrator()\n"
                 "    {\n"
                 "        registerProtobufTypes();\n"
                 "    }\n"
                 "};\n"
                 "\n"
                 "\n"
                 "static volatile Registrator registrator;\n"
                 "\n"
                 "} // namespace\n\n");

    for (const auto* e : enums)
        generateEnumOperator(e, source);

    for (const auto* msg : messages)
        generateMessageOperator(msg, source);

    trimTrailingNuls(source_content);
    writeFileIfChanged(source_name, source_content, context);

    return true;
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    MetatypeGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
