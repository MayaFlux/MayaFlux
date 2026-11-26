#pragma once

namespace MayaFlux::Reflect {

/**
 * @brief A function by name and its arguments as text, keyed by parameter name.
 *
 * What a std::function cannot say about itself, so it can be written down and
 * made again by a FunctionTable. Without arguments it is a name alone.
 */
using FunctionCall = std::pair<std::string, std::map<std::string, std::string, std::less<>>>;

}
