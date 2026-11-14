#pragma once

#include "FactoryTable.hpp"

namespace MayaFlux::Portal::Forma {

/** @brief Attribute names of an on element that the document owns. */
inline constexpr std::string_view action_event_attribute { "event" };
inline constexpr std::string_view action_button_attribute { "button" };
inline constexpr std::string_view action_name_attribute { "action" };

/** @brief The pointer events an action can be attached to. */
enum class PointerEvent : uint8_t {
    Press,
    Release,
    Drag,
    Move,
    Enter,
    Leave,
    Scroll,
};

/**
 * @struct ActionEvent
 * @brief What an action is told when its event happens on an instance.
 *
 * The references are valid for the duration of the call. position is the
 * pointer in NDC, except for Enter and Leave where it is zero. scroll is the
 * wheel delta for Scroll and zero otherwise.
 */
struct ActionEvent {
    Layer& layer;
    Context& context;
    uint32_t element;
    std::string_view instance;
    PointerEvent event;
    glm::vec2 position;
    glm::vec2 scroll;
};

/** @brief Code a host registers by name to run when an instance's event happens. */
using Action = std::function<void(const ActionEvent&)>;

/**
 * @class ActionTable
 * @brief Actions by name, which a document attaches to events and never defines.
 *
 * A plain value like every table here. An instance of a document may hold on
 * children, each naming an event, an optional button and an action. The names
 * are resolved when the document is checked, so an unknown action fails before
 * anything is placed. An action may take parameters, written as further
 * attributes of the on element and decoded strictly through a described type.
 *
 * Because an action is subscribed beside an element's own wiring, it never
 * replaces a Form's built in interaction.
 *
 * Usage:
 * @code
 * ActionTable<IO::XmlElementView> actions;
 * actions.expose_typed<PrintInputs>("print", {}, [](const PrintInputs& input) {
 *     return Action([message = input.message](const ActionEvent&) { std::cout << message << '\n'; });
 * });
 * @endcode
 *
 * @tparam Source Element type with a Reflect::RecordAccess specialization.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
class ActionTable {
public:
    /** @brief Decodes an on element into the action to run, or nullopt with an error. */
    using Maker = std::function<std::optional<Action>(const Source&, std::string&)>;

    /**
     * @brief Register an action that takes no parameters.
     *
     * The on element may carry only its event, button and action attributes.
     */
    void expose(std::string name, Action action)
    {
        m_makers.insert_or_assign(name,
            [action = std::move(action)](const Source& on, std::string& error) -> std::optional<Action> {
                std::optional<std::string> unexpected;
                Access::for_each_attribute(on, [&](std::string_view attribute, std::string_view) {
                    if (attribute == action_event_attribute || attribute == action_button_attribute
                        || attribute == action_name_attribute) {
                        return true;
                    }
                    unexpected = std::string(attribute);
                    return false;
                });
                if (unexpected) {
                    error = Reflect::located_message(Access::location(on),
                        "<on> has an unsupported attribute " + *unexpected);
                    return std::nullopt;
                }
                return action;
            });
    }

    /**
     * @brief Register an action whose parameters are a described type.
     *
     * The attributes of the on element, other than event, button and action,
     * are decoded strictly into Input, and make builds the action from it.
     *
     * @param binding Binding for Input, configured with any required names.
     * @param make Callable taking the decoded Input and returning the Action.
     */
    template <Reflect::Described Input>
    void expose_typed(std::string name, Reflect::RecordBinding<Input, Source, Access> binding,
        std::type_identity_t<std::function<Action(const Input&)>> make)
    {
        binding.ignore({ action_event_attribute, action_button_attribute, action_name_attribute });
        m_makers.insert_or_assign(name,
            [binding = std::move(binding), make = std::move(make)](
                const Source& on, std::string& error) -> std::optional<Action> {
                uint64_t where {};
                const auto input = binding.read(on, error, &where);
                if (!input) {
                    error = Reflect::located_message(where, error);
                    return std::nullopt;
                }
                return make(*input);
            });
    }

    /** @brief Add every action of another table under a name prefix. */
    bool merge(const ActionTable& other, std::string_view prefix, std::string& error, bool replace = false)
    {
        return m_makers.merge_prefixed(other.m_makers, prefix, error, replace);
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_makers.find(name) != m_makers.end();
    }

    /** @brief Registered names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_makers)
            result.push_back(entry.first);
        return result;
    }

    /** @brief The maker of this name, or nullptr. */
    [[nodiscard]] const Maker* find(std::string_view name) const
    {
        const auto found = m_makers.find(name);
        return found == m_makers.end() ? nullptr : &found->second;
    }

private:
    Memory::FlatMap<std::string, Maker, std::less<>> m_makers;
};

}
