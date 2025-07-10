#include "Utils.hpp"

namespace MayaFlux::Utils {

std::any safe_get_parameter(const std::string& parameter_name, const std::map<std::string, std::any> parameters)
{
    auto it = parameters.find(parameter_name);
    if (it != parameters.end()) {
        return it->second;
    }
    std::cerr << "Parameter not found: " << parameter_name << std::endl;
    return {};
}
}
