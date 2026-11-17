#pragma once

#include <expected>

namespace Lila {

/**
 * @brief A value, or the reason there is none.
 */
template <typename T = void>
using Result = std::expected<T, std::string>;

} // namespace Lila
