#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Portal/Forma/Primitives/FormFactory.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Geometry.hpp"
#include "MayaFlux/Transitive/Memory/FlatMap.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct InteractionSource
 * @brief What an interaction provider may use: the placement bounds, when there are any.
 */
struct InteractionSource {
    std::optional<Kinesis::AABB2D> placement;
};

/** @brief The interaction registration of a Form, run once after its element exists. */
template <typename T>
using InteractionFn = std::function<void(Context&, uint32_t, std::shared_ptr<MappedState<T>>)>;

/** @brief Produces the interaction of a Form of state type T, or nullopt with an error. */
template <typename T>
using InteractionProvider = std::function<std::optional<InteractionFn<T>>(const InteractionSource&, std::string&)>;

/**
 * @class InteractionTable
 * @brief Interaction providers for one state type, by name.
 *
 * A plain value, with nothing registered until its builder adds it. An
 * factory that lets a document choose its interaction declares a name among
 * its described inputs and resolves it here, so interaction is selected by the
 * document and implemented in C++.
 *
 * @tparam T State type the handlers write.
 */
template <typename T>
class InteractionTable {
public:
    /** @brief Register a provider, replacing any of the same name. */
    void expose(std::string name, InteractionProvider<T> provider)
    {
        m_providers.insert_or_assign(name, std::move(provider));
    }

    /** @brief Add another table under a prefix, as FlatMap::merge_prefixed. */
    bool merge(const InteractionTable& other, std::string_view prefix,
        std::string& error, bool replace = false)
    {
        return m_providers.merge_prefixed(other.m_providers, prefix, error, replace);
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_providers.find(name) != m_providers.end();
    }

    /** @brief Registered names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_providers)
            result.push_back(entry.first);
        return result;
    }

    /** @brief Run the provider of this name on a source. */
    [[nodiscard]] std::optional<InteractionFn<T>> resolve(
        std::string_view name, const InteractionSource& source, std::string& error) const
    {
        const auto provider = m_providers.find(name);
        if (provider == m_providers.end()) {
            error = "no registered interaction " + std::string(name);
            return std::nullopt;
        }
        return std::invoke(provider->second, source, error);
    }

    /**
     * @brief Resolve a name onto a Form's interaction.
     *
     * An empty name leaves the Form as it is and succeeds, so an input that
     * does not name an interaction needs no check by the caller.
     *
     * @param placement Bounds the provider may use, when the placement has any.
     * @param error Receives the reason when false is returned.
     */
    bool attach(Geometry::Form<T>& form, std::string_view name,
        std::optional<Kinesis::AABB2D> placement, std::string& error) const
    {
        if (name.empty())
            return true;
        const auto interaction = resolve(name, InteractionSource { .placement = placement }, error);
        if (!interaction)
            return false;
        form.wire = *interaction;
        return true;
    }

private:
    Memory::FlatMap<std::string, InteractionProvider<T>, std::less<>> m_providers;
};

/** @brief Provider for an interaction that needs nothing from the placement. */
template <typename T>
[[nodiscard]] InteractionProvider<T> fixed_interaction(InteractionFn<T> interaction)
{
    return [interaction = std::move(interaction)](const InteractionSource&, std::string&)
               -> std::optional<InteractionFn<T>> { return interaction; };
}

/**
 * @brief Provider for an interaction made from the placement bounds.
 *
 * The provider reports an error when the placement has no bounds, so the
 * callable never has to check.
 *
 * @param make Callable taking the bounds and returning the interaction.
 */
template <typename T>
[[nodiscard]] InteractionProvider<T> bounded_interaction(
    std::function<InteractionFn<T>(const Kinesis::AABB2D&)> make)
{
    return [make = std::move(make)](const InteractionSource& source, std::string& error)
               -> std::optional<InteractionFn<T>> {
        if (!source.placement) {
            error = "interaction needs placement bounds";
            return std::nullopt;
        }
        return make(*source.placement);
    };
}

/** @brief Provider whose handler flips a bool on a left press. Register it under any name. */
[[nodiscard]] inline InteractionProvider<bool> press_flip_interaction()
{
    return [](const InteractionSource&, std::string&) -> std::optional<InteractionFn<bool>> {
        return InteractionFn<bool>(Geometry::press_flip());
    };
}

}
