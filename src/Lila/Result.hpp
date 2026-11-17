#pragma once

#include <expected>

namespace Lila {

/**
 * @brief A value, or the reason there is none.
 */
template <typename T = void>
using Result = std::expected<T, std::string>;

/**
 * @brief Success, or a view of the reason that stays valid until the next failing
 *        call on the same thread.
 *
 * The public calls of Lila return this so that no string allocated inside the
 * Lila library is released outside it. Copy the reason to keep it.
 */
using Status = std::expected<void, std::string_view>;

} // namespace Lila
