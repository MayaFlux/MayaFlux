#include "FunctionSidecar.hpp"

#include "Sidecar.hpp"

namespace MayaFlux::Reflect::Tool {

std::string render_function_sidecar(const HeaderFunctions& functions)
{
    std::vector<const FunctionRequest*> sorted;
    sorted.reserve(functions.functions.size());
    for (const FunctionRequest& function : functions.functions) {
        sorted.push_back(&function);
    }
    std::ranges::sort(sorted, [](const FunctionRequest* a, const FunctionRequest* b) {
        return std::tie(a->name, a->expression) < std::tie(b->name, b->expression);
    });

    std::string out;
    out += "#pragma once\n\n";
    out += "#include \"MayaFlux/Transitive/Reflect/Describe.hpp\"\n\n";
    out += "#ifndef MAYAFLUX_REFLECT_SCAN\n\n";
    out += "namespace MayaFlux::Reflect::Generated {\n\n";
    for (const FunctionRequest* function : sorted) {
        out += "template <>\n";
        out += "struct Signature<" + function->expression + "> {\n";
        out += "    static constexpr std::string_view name = \"" + function->name + "\";\n";
        out += "    static constexpr std::size_t required = " + std::to_string(function->required) + ";\n";
        out += "    static constexpr std::array<std::string_view, " + std::to_string(function->parameters.size())
            + "> parameters {";
        for (std::size_t i = 0; i < function->parameters.size(); ++i) {
            out += i == 0 ? " " : ", ";
            out += "\"" + function->parameters[i] + "\"";
        }
        out += function->parameters.empty() ? "};\n" : " };\n";
        out += "\n    template <std::size_t K, typename Values>\n";
        out += "    static decltype(auto) call(Values& values)\n    {\n";
        for (std::size_t count = function->required; count <= function->parameters.size(); ++count) {
            out += count == function->required ? "        if constexpr (K == " : "        } else if constexpr (K == ";
            out += std::to_string(count) + ") {\n";
            out += "            return " + function->callee + "(";
            for (std::size_t i = 0; i < count; ++i) {
                out += i == 0 ? "" : ", ";
                out += "std::get<" + std::to_string(i) + ">(values)";
            }
            out += ");\n";
        }
        out += "        } else {\n";
        out += "            static_assert(K != K, \"the sidecar has no call with that many arguments\");\n";
        out += "        }\n";
        out += "    }\n";
        out += "};\n\n";
    }
    out += "}\n\n#endif\n";
    return out;
}

unsigned last_function_line(const HeaderFunctions& functions)
{
    unsigned last = 0;
    for (const FunctionRequest& function : functions.functions) {
        last = std::max(last, function.line);
    }
    return last;
}

std::vector<std::string> check_function_sidecars(
    const FunctionMap& requests,
    const std::vector<std::filesystem::path>& headers,
    const std::vector<std::filesystem::path>& existing)
{
    std::vector<std::string> problems;

    for (const auto& header : headers) {
        const auto found = requests.find(header);
        if (found == requests.end()) {
            problems.push_back(header.string() + ": contains MF_DESCRIBE_FUNCTION text but produced no request");
            continue;
        }

        const std::filesystem::path sidecar = sidecar_path(header, function_sidecar_suffix);
        const auto current = read_text(sidecar);
        if (!current) {
            problems.push_back(sidecar.string() + ": missing sidecar");
        } else if (*current != render_function_sidecar(found->second)) {
            problems.push_back(sidecar.string() + ": differs from what the generator would write");
        }

        if (const auto problem = check_include(header, last_function_line(found->second), function_sidecar_suffix)) {
            problems.push_back(*problem);
        }
    }

    for (const auto& sidecar : existing) {
        const std::string name = sidecar.filename().string();
        const std::string stem = name.substr(0, name.size() - function_sidecar_suffix.size());
        const std::filesystem::path header = sidecar.parent_path() / (stem + ".hpp");
        if (!requests.contains(header)) {
            problems.push_back(sidecar.string() + ": orphaned, its header has no MF_DESCRIBE_FUNCTION request");
        }
    }
    return problems;
}

}
