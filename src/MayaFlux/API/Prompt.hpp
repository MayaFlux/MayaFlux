#pragma once

#include "MayaFlux/Portal/Forma/Primitives/Picker.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Prompt.hpp"
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

namespace MayaFlux::detail {

/**
 * @brief Open a picker and wait for the choice.
 *
 * Blocks until the user chooses or closes the window, so it must not be called from
 * the event thread. Portal::Forma must be initialized and the engine running.
 *
 * @return The chosen item, or nothing when the window was closed or @p items is empty.
 */
template <typename T>
std::optional<T> pick_one(std::string title, std::vector<T> items,
    std::function<std::string(const T&)> label)
{
    auto promise = std::make_shared<std::promise<std::optional<T>>>();
    auto future = promise->get_future();

    Portal::Forma::pick<T>(std::move(title), std::move(items), std::move(label),
        [promise](std::optional<T> chosen) { promise->set_value(std::move(chosen)); });

    return Parallel::await_future(std::move(future));
}

/**
 * @brief Open a text prompt and wait for the text.
 *
 * Blocks like pick_one. Only text the validator accepts is returned.
 *
 * @return The text, or nothing when the prompt was cancelled.
 */
inline std::optional<std::string> ask_one(std::string title, std::string initial,
    Portal::Forma::AskValidator validator)
{
    auto promise = std::make_shared<std::promise<std::optional<std::string>>>();
    auto future = promise->get_future();

    Portal::Forma::ask(std::move(title), std::move(initial), std::move(validator),
        [promise](std::optional<std::string> text) { promise->set_value(std::move(text)); });

    return Parallel::await_future(std::move(future));
}

} // namespace MayaFlux::detail
