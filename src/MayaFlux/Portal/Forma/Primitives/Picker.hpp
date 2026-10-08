#pragma once

namespace MayaFlux::Portal::Forma {

/**
 * @brief Receives the chosen index, or nullopt when the window was closed without a choice.
 */
using PickCallback = std::function<void(std::optional<size_t>)>;

/**
 * @brief Open a window of clickable rows, one per label, and report the choice.
 *
 * Requires Portal::Forma to be initialized and the engine running. The
 * callback fires exactly once, on the event thread, so it must not block.
 * Selecting a row hides the window and delivers its index. Closing the
 * window delivers nullopt. Lists longer than ten rows scroll with the mouse
 * wheel. An empty list delivers nullopt immediately.
 *
 * @param title    Window title.
 * @param labels   Row captions, in display order.
 * @param callback Invoked once with the chosen index or nullopt.
 */
MAYAFLUX_API void pick(
    std::string title,
    std::vector<std::string> labels,
    PickCallback callback);

/**
 * @brief Typed form of pick: list @p items by @p label and receive the chosen item.
 *
 * Same behaviour as pick. The callback receives a copy of the
 * chosen item, or nullopt if the window was closed without a choice.
 *
 * @tparam T       Item type, deduced from @p items.
 * @param title    Window title.
 * @param items    Candidates, in display order.
 * @param label    Maps an item to its row caption.
 * @param callback Invoked once with the chosen item or nullopt.
 */
template <typename T>
void pick(
    std::string title,
    std::vector<T> items,
    std::type_identity_t<std::function<std::string(const T&)>> label,
    std::type_identity_t<std::function<void(std::optional<T>)>> callback)
{
    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const auto& item : items) {
        labels.push_back(label(item));
    }

    pick(std::move(title), std::move(labels),
        [items = std::move(items), callback = std::move(callback)](std::optional<size_t> index) {
            callback(index ? std::optional<T>(items.at(*index)) : std::nullopt);
        });
}

} // namespace MayaFlux::Portal::Forma
