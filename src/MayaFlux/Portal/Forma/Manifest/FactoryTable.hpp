#pragma once

#include "MayaFlux/IO/Composite/DescribedRecords.hpp"
#include "MayaFlux/IO/GlmTextCodec.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Geometry.hpp"
#include "MayaFlux/Portal/Forma/Tend.hpp"
#include "MayaFlux/Transitive/Memory/FlatMap.hpp"
#include "MayaFlux/Transitive/Reflect/AttributeChecks.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Attribute names a document owns on an instance.
 *
 * A table skips the two names and every attribute beginning with the prefix
 * when it decodes a placement, so a factory never lists them. The driver
 * reads the prefixed attributes itself.
 */
inline constexpr std::string_view reserved_id_attribute { "id" };
inline constexpr std::string_view reserved_template_attribute { "template" };
inline constexpr std::string_view reserved_prefix { "mf:" };

/**
 * @brief The child element of an instance that attaches an action to an event.
 *
 * A table skips it when it decodes a placement. The driver reads it.
 */
inline constexpr std::string_view reserved_action_element { "on" };

/**
 * @class FactoryTable
 * @brief Named factories that turn a template's factory element and an instance into placed Forma elements, as a plain value.
 *
 * A table is a plain value. It holds only what its builder registers, and
 * nothing is pre-registered or privileged: a host, a library or a user each
 * build their own and combine them with merge(). A factory is a
 * preparation that decodes everything and returns a Placer, so a document can
 * be checked completely before anything is placed.
 *
 * Source is any element type with a Reflect::RecordAccess specialization and
 * value semantics, so the same table serves XML, a script or any other
 * document tree.
 *
 * Two ways to register:
 * - expose_typed: described input and placement types. The factory also gets
 *   a bulk path that decodes its inputs once and makes one element per row of
 *   typed placements.
 * - expose_placement: any placement at all, with any number of elements,
 *   composed primitives or bindings, onto the Surface it is given.
 *
 * Errors name the row of the failing element.
 *
 * Usage:
 * @code
 * FactoryTable<IO::XmlElementView> mine;
 * mine.expose_typed<float>("dial", dial_input, dial_placement, make_dial, dial_initial);
 * FactoryTable<IO::XmlElementView> shared;
 * std::string error;
 * shared.merge(mine, "studio.", error);
 * @endcode
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
class FactoryTable {
public:
    /**
     * @brief Places one prepared element, or several, onto a Surface.
     *
     * May be run more than once. Returns the id of the element the instance is
     * anchored on, or nullopt with an error. A placement of several elements
     * returns the one the others belong with.
     */
    using Placer = std::function<std::optional<uint32_t>(Surface&, std::string&)>;

    /** @brief Decodes a factory element and an instance into a Placer, or nullopt with an error. */
    using Instantiator = std::function<std::optional<Placer>(
        const Source&, const Source&, std::string&)>;

    /** @brief Decodes a factory element and typed placement rows into one Placer per row. */
    using BulkInstantiator = std::function<std::optional<std::vector<Placer>>(
        const Source&, const Kakshya::CompositeArray&, std::string&)>;

    /**
     * @brief Register a whole placement.
     *
     * The Instantiator returns a Placer that receives the Surface. It may place
     * any number of elements, run composed primitives, or create bindings.
     * Registering an existing name replaces it.
     */
    void expose_placement(std::string name, Instantiator instantiator)
    {
        m_bulk.erase(name);
        m_factories.insert_or_assign(name, std::move(instantiator));
    }

    /**
     * @brief Expose a factory whose inputs and placement are described types.
     *
     * The inputs are read from the factory element and the placement from the
     * instance, both strictly through their described members. The document's own
     * attribute names are skipped on the placement. Initial state and the
     * post-placement hook receive the decoded values, so one factory serves a
     * single instance and a bulk placement alike.
     *
     * @param input Binding for the factory element.
     * @param placement Binding for the instance.
     * @param factory Callable (const Input&, const Placement&, error) returning
     * std::optional of Geometry::Form of T.
     * @param initial Callable (const Input&, const Placement&, error) returning
     * std::optional of T, or a pointer to a member of Input or Placement whose
     * value is the initial state.
     * @param placed Optional hook called with the decoded values and the placed Tend.
     */
    template <typename T, Reflect::Described Input, Reflect::Described Placement,
        typename Factory, typename Initial>
    void expose_typed(std::string name,
        Reflect::RecordBinding<Input, Source, Access> input,
        Reflect::RecordBinding<Placement, Source, Access> placement,
        Factory factory, Initial initial,
        std::type_identity_t<std::function<void(const Input&, const Placement&, Tend<T>&)>> placed = {})
    {
        placement.ignore({ reserved_id_attribute, reserved_template_attribute });
        placement.ignore_prefix(std::string(reserved_prefix));
        placement.ignore_child(std::string(reserved_action_element));

        using Maker = std::function<std::optional<Placer>(
            const Input&, const Placement&, std::string&)>;

        Maker maker = [factory = std::move(factory), initial = std::move(initial),
                          placed = std::move(placed)](
                          const Input& decoded_input, const Placement& decoded_placement,
                          std::string& error) -> std::optional<Placer> {
            auto geometry = std::invoke(factory, decoded_input, decoded_placement, error);
            if (!geometry) {
                if (error.empty())
                    error = "could not create the geometry";
                return std::nullopt;
            }
            std::optional<T> value;
            if constexpr (std::is_member_object_pointer_v<Initial>) {
                if constexpr (std::is_invocable_v<Initial, const Input&>)
                    value = static_cast<T>(std::invoke(initial, decoded_input));
                else
                    value = static_cast<T>(std::invoke(initial, decoded_placement));
            } else {
                value = std::invoke(initial, decoded_input, decoded_placement, error);
            }
            if (!value) {
                if (error.empty())
                    error = "could not create the initial state";
                return std::nullopt;
            }
            std::function<void(Tend<T>&)> after;
            if (placed) {
                after = [placed, decoded_input, decoded_placement](Tend<T>& tend) {
                    placed(decoded_input, decoded_placement, tend);
                };
            }
            return make_placer<T>(std::move(*geometry), std::move(*value), std::move(after));
        };

        Instantiator single = [input, placement, maker](
                               const Source& factory_element, const Source& instance,
                               std::string& error) -> std::optional<Placer> {
            uint64_t where {};
            const auto decoded_input = input.read(factory_element, error, &where);
            if (!decoded_input) {
                error = located(where, error);
                return std::nullopt;
            }
            const auto decoded_placement = placement.read(instance, error, &where);
            if (!decoded_placement) {
                error = located(where, error);
                return std::nullopt;
            }
            auto placer = maker(*decoded_input, *decoded_placement, error);
            if (!placer)
                error = located(Access::location(instance), error);
            return placer;
        };

        BulkInstantiator bulk = [input, maker](
                                 const Source& factory_element, const Kakshya::CompositeArray& rows,
                                 std::string& error) -> std::optional<std::vector<Placer>> {
            uint64_t where {};
            const auto decoded_input = input.read(factory_element, error, &where);
            if (!decoded_input) {
                error = located(where, error);
                return std::nullopt;
            }
            const auto placements = IO::records_of<Placement>(rows, error);
            if (!placements) {
                error = located(Access::location(factory_element), error);
                return std::nullopt;
            }

            std::vector<Placer> placers;
            placers.reserve(placements->size());
            for (const Placement& decoded_placement : *placements) {
                auto placer = maker(*decoded_input, decoded_placement, error);
                if (!placer) {
                    error = located(Access::location(factory_element), error);
                    return std::nullopt;
                }
                placers.push_back(std::move(*placer));
            }
            return placers;
        };

        m_factories.insert_or_assign(name, std::move(single));
        m_bulk.insert_or_assign(name, std::move(bulk));
    }

    /**
     * @brief Add every factory of another table under a name prefix.
     * @param prefix Prepended to each name; empty keeps the names.
     * @param error Receives the first colliding name when false is returned.
     * @param replace True to overwrite existing names instead of failing.
     * @return True on success. A failed merge leaves this table unchanged.
     */
    bool merge(const FactoryTable& other, std::string_view prefix,
        std::string& error, bool replace = false)
    {
        if (!m_factories.merge_prefixed(other.m_factories, prefix, error, replace))
            return false;
        for (const auto& entry : other.m_factories) {
            const std::string target = std::string(prefix) + entry.first;
            const auto bulk = other.m_bulk.find(entry.first);
            if (bulk != other.m_bulk.end()) {
                m_bulk.insert_or_assign(target, bulk->second);
            } else {
                m_bulk.erase(target);
            }
        }
        return true;
    }

    /** @brief Whether a factory of this name is registered. */
    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_factories.find(name) != m_factories.end();
    }

    /** @brief Registered names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_factories)
            result.push_back(entry.first);
        return result;
    }

    /** @brief The factory of this name, or nullptr. */
    [[nodiscard]] const Instantiator* find(std::string_view name) const
    {
        const auto found = m_factories.find(name);
        return found == m_factories.end() ? nullptr : &found->second;
    }

    /** @brief The bulk path of this name, or nullptr when it was not registered with expose_typed. */
    [[nodiscard]] const BulkInstantiator* find_bulk(std::string_view name) const
    {
        const auto found = m_bulk.find(name);
        return found == m_bulk.end() ? nullptr : &found->second;
    }

    /** @brief The message prefixed with the row it concerns. */
    [[nodiscard]] static std::string located(uint64_t row, std::string_view message)
    {
        return Reflect::located_message(row, message);
    }

private:
    template <typename T>
    static Placer make_placer(Geometry::Form<T> form, T initial,
        std::function<void(Tend<T>&)> after)
    {
        return [form = std::move(form), initial = std::move(initial),
                   after = std::move(after)](Surface& surface, std::string& error) -> std::optional<uint32_t> {
            Tend<T> tend {
                .form = form,
                .initial = initial,
            };
            tend.place(surface);
            if (!tend.result) {
                error = "could not place the element";
                return std::nullopt;
            }
            if (after)
                after(tend);
            return tend.result->element.id;
        };
    }

    Memory::FlatMap<std::string, Instantiator, std::less<>> m_factories;
    Memory::FlatMap<std::string, BulkInstantiator, std::less<>> m_bulk;
};

}
