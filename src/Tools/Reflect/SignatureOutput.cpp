#include "SignatureOutput.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    std::string joined(const std::vector<std::string>& lines)
    {
        std::string out;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (i != 0) {
                out += '\n';
            }
            out += lines[i];
        }
        return out;
    }

    std::string include_line(const std::filesystem::path& header)
    {
        return "#include \"" + header.generic_string() + "\"";
    }

    std::string join_parameters(const ConstructorSignature& constructor, bool with_defaults)
    {
        std::string out;
        for (std::size_t i = 0; i < constructor.parameters.size(); ++i) {
            const SignatureParameter& parameter = constructor.parameters[i];
            if (i != 0) {
                out += ", ";
            }
            out += parameter.type + " " + parameter.name;
            if (with_defaults && parameter.default_value) {
                out += " = " + *parameter.default_value;
            }
        }
        return out;
    }

    std::string join_arguments(const ConstructorSignature& constructor)
    {
        std::string out;
        for (std::size_t i = 0; i < constructor.parameters.size(); ++i) {
            const std::string& name = constructor.parameters[i].name;
            if (i != 0) {
                out += ", ";
            }
            out += "std::forward<decltype(" + name + ")>(" + name + ")";
        }
        return out;
    }

}

SignatureFiles render_signature_files(
    const std::vector<SignatureClass>& classes, const std::filesystem::path& source_root)
{
    std::set<std::filesystem::path> signature_headers;
    std::vector<std::string> class_includes { "#pragma once", "" };
    std::vector<std::string> declarations;
    std::vector<std::string> definitions;

    for (const SignatureClass& item : classes) {
        if (item.abstract) {
            continue;
        }
        signature_headers.insert(item.includes.begin(), item.includes.end());
        class_includes.push_back(include_line(item.entry.header.lexically_relative(source_root)));

        constexpr std::string_view root = "MayaFlux::";
        const std::string relative = item.entry.type_name.starts_with(root)
            ? item.entry.type_name.substr(root.size())
            : item.entry.type_name;
        const std::string result = "std::shared_ptr<" + relative + ">";
        for (const ConstructorSignature& constructor : item.constructors) {
            declarations.push_back("    " + result + " " + item.entry.name + "("
                + join_parameters(constructor, true) + ");");
            definitions.push_back(result + " Creator::" + item.entry.name + "(" + join_parameters(constructor, false) + ")");
            definitions.emplace_back("{");
            definitions.push_back("    auto obj = std::make_shared<" + item.entry.type_name + ">("
                + join_arguments(constructor) + ");");
            definitions.push_back("    MF_LIVE_EXPOSE_NAMED(\"" + item.entry.name + "\", obj);");
            definitions.emplace_back("    return obj;");
            definitions.emplace_back("}");
            definitions.emplace_back("");
        }
    }

    std::vector<std::string> includes { "#pragma once", "" };
    for (const std::filesystem::path& header : signature_headers) {
        includes.push_back(include_line(header));
    }

    return SignatureFiles {
        .includes = joined(includes) + "\n",
        .classes = joined(class_includes) + "\n",
        .declarations = joined(declarations) + "\n",
        .definitions = joined(definitions),
    };
}

}
