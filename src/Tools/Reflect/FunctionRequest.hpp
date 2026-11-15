#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct FunctionRequest
 * @brief One described function: how to name it as a template argument, and its parameter names.
 *
 * expression is the pointer to the function as it must be written in a specialization of
 * Reflect::Generated::Signature, with a cast when the name is overloaded. callee is the name a
 * call is written with, with the template arguments of a specialization, so the function's own
 * default arguments apply. required counts the
 * leading parameters that have no default. line is the line after which the sidecar include may
 * appear: the closing brace of the outermost namespace holding the marker.
 */
struct FunctionRequest {
    std::string name;
    std::string expression;
    std::string callee;
    std::vector<std::string> parameters;
    std::size_t required {};
    unsigned line {};
};

/** @brief Every function described by the MF_DESCRIBE_FUNCTION markers of one header. */
struct HeaderFunctions {
    std::filesystem::path header;
    std::vector<FunctionRequest> functions;
};

using FunctionMap = std::map<std::filesystem::path, HeaderFunctions>;

}
