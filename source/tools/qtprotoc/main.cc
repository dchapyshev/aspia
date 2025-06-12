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
#include <google/protobuf/io/printer.h>

//--------------------------------------------------------------------------------------------------
std::string getQualifiedCppTypeName(const std::string& full_name)
{
    std::string result;

    for (char c : full_name)
        result += (c == '.') ? "::" : std::string(1, c);

    return result;
}

//--------------------------------------------------------------------------------------------------
void collectAllTypes(const google::protobuf::Descriptor* descriptor,
                     std::vector<std::string>* message_types,
                     std::vector<const google::protobuf::EnumDescriptor*>* enums)
{
    message_types->push_back(getQualifiedCppTypeName(descriptor->full_name()));

    for (int i = 0; i < descriptor->enum_type_count(); ++i)
        enums->push_back(descriptor->enum_type(i));

    for (int i = 0; i < descriptor->nested_type_count(); ++i)
        collectAllTypes(descriptor->nested_type(i), message_types, enums);
}

//--------------------------------------------------------------------------------------------------
void collectFileTypes(const google::protobuf::FileDescriptor* file,
                      std::vector<std::string>* message_types,
                      std::vector<const google::protobuf::EnumDescriptor*>* enums)
{
    for (int i = 0; i < file->message_type_count(); ++i)
        collectAllTypes(file->message_type(i), message_types, enums);

    for (int i = 0; i < file->enum_type_count(); ++i)
        enums->push_back(file->enum_type(i));
}

//--------------------------------------------------------------------------------------------------
std::string makeIncludeGuardMacro(const std::string& filename)
{
    std::string guard = filename;
    std::replace(guard.begin(), guard.end(), '.', '_');
    std::replace(guard.begin(), guard.end(), '/', '_');
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    return "PROTO_" + guard;
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
    std::size_t last_dot = header_name.rfind('.');
    if (last_dot != std::string::npos)
        header_name.replace(last_dot, std::string::npos, ".h");
    else
        header_name += ".h";

    std::string source_name = file->name();
    if (last_dot != std::string::npos)
        source_name.replace(last_dot, std::string::npos, ".cc");
    else
        source_name += ".cc";

    std::string pb_header = file->name();
    std::size_t dot_pos = pb_header.rfind(".proto");
    if (dot_pos != std::string::npos)
        pb_header.replace(dot_pos, 6, ".pb.h");
    else
        pb_header += ".pb.h";

    std::vector<std::string> message_types;
    std::vector<const google::protobuf::EnumDescriptor*> enum_descriptors;

    collectFileTypes(file, &message_types, &enum_descriptors);

    std::set<std::string> all_types;
    all_types.insert(message_types.begin(), message_types.end());
    for (const auto* e : enum_descriptors)
        all_types.insert(getQualifiedCppTypeName(e->full_name()));

    //
    // Header File
    //
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> header_output(context->Open(header_name));
    google::protobuf::io::Printer header(header_output.get(), '$');

    const std::string include_guard = makeIncludeGuardMacro(header_name);

    header.Print("#ifndef $GUARD$\n#define $GUARD$\n", "GUARD", include_guard);
    header.Print("\n");
    header.Print("#include <QMetaType>\n"
                 "#include <QTextStream>\n"
                 "\n");
    header.Print("#include \"proto/$PB_HEADER$\" // IWYU pragma: export\n", "PB_HEADER", pb_header);
    header.Print("\n");

    for (const std::string& type : all_types)
        header.Print("Q_DECLARE_METATYPE($TYPE$)\n", "TYPE", type);
    header.Print("\n");

    for (const auto* e : enum_descriptors)
    {
        const std::string cpp_enum_type = getQualifiedCppTypeName(e->full_name());
        header.Print("QTextStream& operator<<(QTextStream& stream, $TYPE$ value);\n", "TYPE", cpp_enum_type);
    }

    header.Print("\n#endif  // $GUARD$\n", "GUARD", include_guard);

    //
    // Source File
    //
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> source_output(context->Open(source_name));
    google::protobuf::io::Printer source(source_output.get(), '$');

    source.Print("#include \"$HEADER$\"\n\n", "HEADER", header_name);

    source.Print("namespace {\n"
                 "\n"
                 "void registerProtobufTypes()\n"
                 "{\n");
    for (const std::string& type : all_types)
        source.Print("    qRegisterMetaType<$TYPE$>(\"$TYPE$\");\n", "TYPE", type);
    source.Print("}\n\n"
                 "struct Registrator\n"
                 "{\n"
                 "    Registrator()\n"
                 "    {\n"
                 "        registerProtobufTypes();\n"
                 "    }\n"
                 "};\n"
                 "\n"
                 "Registrator registrator;\n"
                 "\n"
                 "} // namespace\n\n");

    for (const auto* e : enum_descriptors)
    {
        const std::string cpp_enum_type = getQualifiedCppTypeName(e->full_name());

        source.Print("QTextStream& operator<<(QTextStream& stream, $TYPE$ value)\n", "TYPE", cpp_enum_type);
        source.Print("{\n");
        source.Print("    switch (value)\n"
                     "    {\n");

        for (int i = 0; i < e->value_count(); ++i)
        {
            const auto* val = e->value(i);
            std::string full_enum_scope = getQualifiedCppTypeName(
                e->containing_type() ? e->containing_type()->full_name() : e->full_name());
            std::string full_enum_const = full_enum_scope + "::" + val->name();

            source.Print("        case $CONST$:\n", "CONST", full_enum_const);
            source.Print("            return stream << \"$CONST$\";\n", "CONST", val->name());
        }

        source.Print("        default:\n");
        source.Print("            return stream << \"$TYPE$(unknown)\";\n", "TYPE", cpp_enum_type);
        source.Print("    }\n");
        source.Print("}\n");
        source.Print("\n");
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    MetatypeGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
