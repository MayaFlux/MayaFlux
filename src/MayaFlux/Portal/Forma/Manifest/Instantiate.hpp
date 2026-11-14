#pragma once

#include "ActionTable.hpp"

namespace MayaFlux::Portal::Forma {

/** @brief Surfaces by the document name a host binds them to. */
using SurfaceMap = std::map<std::string, Surface, std::less<>>;

/**
 * @struct PlacedInstance
 * @brief Where an instance of a document ended up.
 */
struct PlacedInstance {
    /// @brief The document's surface id, a key of the SurfaceMap that was given.
    std::string surface;

    /// @brief The id of the element the instance is anchored on, in that surface's Layer.
    uint32_t element {};
};

/** @brief The placed instances of a document by the id the document gave them. */
using PlacedInstances = std::map<std::string, PlacedInstance, std::less<>>;

namespace Detail {

    /**
     * The document's own attributes on an instance, all beginning with the
     * reserved prefix. They are applied to the placed element through its Layer.
     */
    struct LayerAttributes {
        bool hidden {};
        std::optional<bool> interactive;
        std::optional<std::string> relate;
        bool front {};
        bool back {};
        std::optional<Kinesis::AABB2D> clip;
    };

    /**
     * An action prepared for one event of one instance: the event and button
     * to subscribe to, and the action a host registered, already decoded.
     */
    struct ActionBinding {
        PointerEvent event {};
        IO::MouseButtons button { IO::MouseButtons::Left };
        Action action;
    };

    template <typename Source, typename Access>
    class DocumentWalk {
    public:
        using Table = FactoryTable<Source, Access>;
        using Actions = ActionTable<Source, Access>;
        using Placer = typename Table::Placer;

        struct PreparedInstance {
            std::string surface_id;
            Surface* surface {};
            std::string id;
            uint64_t row {};
            Placer placer;
            LayerAttributes layer;
            std::vector<ActionBinding> actions;
        };

        using Prepared = std::vector<PreparedInstance>;

        DocumentWalk(const Table& table, const Source& root, std::string& error,
            const Actions* actions = nullptr)
            : m_table(table)
            , m_root(root)
            , m_error(error)
            , m_actions(actions)
        {
        }

        bool read_structure()
        {
            if (Access::name(m_root) != "forma")
                return fail(m_root, "root element must be <forma>");
            if (!only_attributes(m_root, { "version", "xmlns" }, "xmlns:") || !check_version())
                return false;

            bool templates_seen = false;
            const bool traversed = Access::for_each_child(m_root, [&](const Source& section) {
                const auto name = Access::name(section);
                if (name == "templates") {
                    if (templates_seen)
                        return fail(section, "<templates> may appear only once");
                    templates_seen = true;
                    return read_templates(section);
                }
                if (name == "surface")
                    return read_surface(section);
                return fail(section, "<" + std::string(name) + "> is not allowed in <forma>");
            });
            return traversed && check_instances();
        }

        std::optional<Prepared> prepare(SurfaceMap& surfaces)
        {
            Prepared prepared;
            for (const SurfaceInstances& entry : m_surfaces) {
                const auto bound = surfaces.find(entry.id);
                if (bound == surfaces.end()) {
                    fail(entry.element, "surface " + entry.id + " is not bound by the host");
                    return std::nullopt;
                }
                for (size_t i = 0; i < entry.instances.size(); ++i) {
                    const Source& instance = entry.instances[i];
                    const InstanceInfo& info = entry.info[i];
                    const Source& factory_element = m_templates.at(info.template_id);
                    const typename Table::Instantiator* decode = m_table.find(Access::name(factory_element));
                    auto placer = std::invoke(*decode, factory_element, instance, m_error);
                    if (!placer) {
                        if (m_error.empty())
                            fail(instance, "could not prepare the instance");
                        return std::nullopt;
                    }
                    std::vector<ActionBinding> actions;
                    for (const ActionSpec& spec : info.actions) {
                        const typename Actions::Maker* make = m_actions->find(spec.action);
                        auto action = std::invoke(*make, spec.element, m_error);
                        if (!action) {
                            if (m_error.empty())
                                fail(spec.element, "could not prepare the action " + spec.action);
                            return std::nullopt;
                        }
                        actions.push_back(ActionBinding {
                            .event = spec.event, .button = spec.button, .action = std::move(*action) });
                    }
                    prepared.push_back(PreparedInstance {
                        .surface_id = entry.id,
                        .surface = &bound->second,
                        .id = info.id,
                        .row = Access::location(instance),
                        .placer = std::move(*placer),
                        .layer = info.layer,
                        .actions = std::move(actions),
                    });
                }
            }
            return prepared;
        }

        std::optional<std::vector<Placer>> prepare_bulk(std::string_view template_id,
            const Kakshya::CompositeArray& rows)
        {
            const auto found = m_templates.find(std::string(template_id));
            if (found == m_templates.end()) {
                m_error = "document has no template " + std::string(template_id);
                return std::nullopt;
            }
            const auto* bulk = m_table.find_bulk(Access::name(found->second));
            if (!bulk) {
                fail(found->second, "factory " + std::string(Access::name(found->second))
                        + " has no bulk placement; register it with expose_typed");
                return std::nullopt;
            }
            auto placers = std::invoke(*bulk, found->second, rows, m_error);
            if (!placers && m_error.empty())
                fail(found->second, "could not prepare instances");
            return placers;
        }

        /**
         * Names every placed element after its instance, then applies the
         * document's Layer attributes: relations first, so a later front or back
         * moves the whole group, then visibility, interaction and clip.
         */
        static bool apply(const Prepared& prepared, const std::vector<uint32_t>& elements,
            std::string& error)
        {
            for (size_t i = 0; i < prepared.size(); ++i) {
                Element* element = prepared[i].surface->layer().get(elements[i]);
                if (!element) {
                    error = Table::located(prepared[i].row, "instance " + prepared[i].id
                            + " placed an element the surface does not hold");
                    return false;
                }
                element->name = prepared[i].id;
            }

            for (size_t i = 0; i < prepared.size(); ++i) {
                const auto& relate = prepared[i].layer.relate;
                if (!relate)
                    continue;
                const auto target = std::ranges::find_if(prepared, [&](const PreparedInstance& other) {
                    return other.surface_id == prepared[i].surface_id && other.id == *relate;
                });
                const size_t j = static_cast<size_t>(target - prepared.begin());
                if (target == prepared.end()
                    || !prepared[i].surface->layer().relate(elements[j], elements[i])) {
                    error = Table::located(prepared[i].row, "could not relate " + prepared[i].id
                            + " to " + *relate);
                    return false;
                }
            }

            for (size_t i = 0; i < prepared.size(); ++i) {
                Layer& layer = prepared[i].surface->layer();
                const LayerAttributes& attributes = prepared[i].layer;
                if (attributes.hidden)
                    layer.set_visible(elements[i], false);
                if (attributes.interactive)
                    layer.set_interactive(elements[i], *attributes.interactive);
                if (attributes.clip)
                    layer.set_clip(elements[i], attributes.clip);
                if (attributes.front)
                    layer.bring_to_front(elements[i]);
                if (attributes.back)
                    layer.send_to_back(elements[i]);
            }

            for (size_t i = 0; i < prepared.size(); ++i) {
                for (const ActionBinding& binding : prepared[i].actions)
                    wire(*prepared[i].surface, elements[i], prepared[i].id, binding);
            }
            return true;
        }

    private:
        struct ActionSpec {
            Source element;
            PointerEvent event {};
            IO::MouseButtons button { IO::MouseButtons::Left };
            std::string action;
        };

        struct InstanceInfo {
            std::string id;
            std::string template_id;
            LayerAttributes layer;
            std::vector<ActionSpec> actions;
        };

        struct SurfaceInstances {
            std::string id;
            Source element;
            std::vector<Source> instances;
            std::vector<InstanceInfo> info;
        };

        const Table& m_table;
        const Source& m_root;
        std::string& m_error;
        const Actions* m_actions;
        Reflect::AttributeIndex<Source, Access> m_ids { std::string(reserved_id_attribute) };
        std::unordered_map<std::string, Source> m_templates;
        std::vector<SurfaceInstances> m_surfaces;

        bool fail(const Source& at, std::string message)
        {
            m_error = Table::located(Access::location(at), message);
            return false;
        }

        bool only_attributes(const Source& source, std::initializer_list<std::string_view> allowed,
            std::string_view allowed_prefix = {})
        {
            return Access::for_each_attribute(source, [&](std::string_view name, std::string_view) {
                if (std::ranges::find(allowed, name) != allowed.end()
                    || (!allowed_prefix.empty() && name.starts_with(allowed_prefix))) {
                    return true;
                }
                return fail(source, "<" + std::string(Access::name(source))
                        + "> has an unsupported attribute " + std::string(name));
            });
        }

        bool check_version()
        {
            const auto text = Access::attribute(m_root, "version");
            if (!text)
                return fail(m_root, "<forma> needs a version attribute");
            uint32_t version {};
            Reflect::TextError reason = Reflect::TextError::Invalid;
            if (!Reflect::TextCodec<uint32_t>::decode(*text, version, reason) || version != 1)
                return fail(m_root, "unsupported Forma document version " + std::string(*text));
            return true;
        }

        std::optional<std::string> claim_id(const Source& source)
        {
            if (!m_ids.claim(source, m_error))
                return std::nullopt;
            return std::string(*Access::attribute(source, reserved_id_attribute));
        }

        bool read_templates(const Source& section)
        {
            if (!only_attributes(section, {}))
                return false;
            return Access::for_each_child(section, [&](const Source& definition) {
                if (Access::name(definition) != "template") {
                    return fail(definition, "<" + std::string(Access::name(definition))
                            + "> is not allowed in <templates>");
                }
                return read_template(definition);
            });
        }

        bool read_template(const Source& definition)
        {
            if (!only_attributes(definition, { reserved_id_attribute }))
                return false;
            const auto id = claim_id(definition);
            if (!id)
                return false;

            std::optional<Source> factory_element;
            size_t count = 0;
            Access::for_each_child(definition, [&](const Source& child) {
                if (!factory_element)
                    factory_element.emplace(child);
                ++count;
                return true;
            });
            if (count != 1)
                return fail(definition, "<template> needs exactly one factory element");

            const auto name = Access::name(*factory_element);
            if (!m_table.contains(name))
                return fail(*factory_element, "no registered factory " + std::string(name));
            m_templates.emplace(*id, *factory_element);
            return true;
        }

        bool read_surface(const Source& surface)
        {
            if (!only_attributes(surface, { reserved_id_attribute }))
                return false;
            const auto id = claim_id(surface);
            if (!id)
                return false;

            SurfaceInstances entry { .id = *id, .element = surface, .instances = {}, .info = {} };
            const bool traversed = Access::for_each_child(surface, [&](const Source& instance) {
                if (Access::name(instance) != "instance") {
                    return fail(instance, "<" + std::string(Access::name(instance))
                            + "> is not allowed in <surface>");
                }
                entry.instances.push_back(instance);
                return true;
            });
            if (traversed)
                m_surfaces.push_back(std::move(entry));
            return traversed;
        }

        bool read_flag(const Source& instance, std::string_view name, std::string_view value, bool& out)
        {
            Reflect::TextError reason = Reflect::TextError::Invalid;
            if (!Reflect::TextCodec<bool>::decode(value, out, reason))
                return fail(instance, "<instance> has an invalid " + std::string(name) + " attribute");
            return true;
        }

        bool read_layer_attributes(const Source& instance, LayerAttributes& out)
        {
            const bool read = Access::for_each_attribute(instance,
                [&](std::string_view name, std::string_view value) {
                    if (!name.starts_with(reserved_prefix))
                        return true;
                    const std::string_view key = name.substr(reserved_prefix.size());
                    if (key == "hidden")
                        return read_flag(instance, name, value, out.hidden);
                    if (key == "front")
                        return read_flag(instance, name, value, out.front);
                    if (key == "back")
                        return read_flag(instance, name, value, out.back);
                    if (key == "interactive") {
                        bool flag {};
                        if (!read_flag(instance, name, value, flag))
                            return false;
                        out.interactive = flag;
                        return true;
                    }
                    if (key == "relate") {
                        if (value.empty())
                            return fail(instance, "<instance> needs an instance id in " + std::string(name));
                        out.relate = std::string(value);
                        return true;
                    }
                    if (key == "clip") {
                        glm::vec4 rect {};
                        Reflect::TextError reason = Reflect::TextError::Invalid;
                        if (!Reflect::TextCodec<glm::vec4>::decode(value, rect, reason))
                            return fail(instance, "<instance> needs four numbers in " + std::string(name));
                        if (!(rect.z > 0.F) || !(rect.w > 0.F))
                            return fail(instance, std::string(name) + " needs a positive width and height");
                        out.clip = Kinesis::AABB2D::from_origin_size({ rect.x, rect.y }, { rect.z, rect.w });
                        return true;
                    }
                    return fail(instance, "<instance> has an unsupported attribute " + std::string(name));
                });
            if (!read)
                return false;
            if (out.front && out.back) {
                return fail(instance, std::string(reserved_prefix) + "front and "
                        + std::string(reserved_prefix) + "back cannot both be set");
            }
            return true;
        }

        bool read_actions(const Source& instance, InstanceInfo& info)
        {
            return Access::for_each_child(instance, [&](const Source& child) {
                if (Access::name(child) != reserved_action_element) {
                    return fail(child, "<" + std::string(Access::name(child))
                            + "> is not allowed in <instance>");
                }
                return read_action(child, info);
            });
        }

        bool read_action(const Source& on, InstanceInfo& info)
        {
            if (!m_actions)
                return fail(on, "<on> needs an action table, and none was given");

            const auto event_text = Access::attribute(on, action_event_attribute);
            if (!event_text)
                return fail(on, "<on> needs an event attribute");
            PointerEvent event {};
            Reflect::TextError reason = Reflect::TextError::Invalid;
            if (!Reflect::TextCodec<PointerEvent>::decode(*event_text, event, reason))
                return fail(on, "<on> has an unsupported event " + std::string(*event_text));

            IO::MouseButtons button = IO::MouseButtons::Left;
            if (const auto button_text = Access::attribute(on, action_button_attribute)) {
                if (event != PointerEvent::Press && event != PointerEvent::Release && event != PointerEvent::Drag)
                    return fail(on, "<on> takes a button only for press, release and drag");
                if (!Reflect::TextCodec<IO::MouseButtons>::decode(*button_text, button, reason)
                    || button == IO::MouseButtons::Unknown) {
                    return fail(on, "<on> has an unsupported button " + std::string(*button_text));
                }
            }

            const auto name = Access::attribute(on, action_name_attribute);
            if (!name)
                return fail(on, "<on> needs an action attribute");
            if (!m_actions->contains(*name))
                return fail(on, "no registered action " + std::string(*name));

            info.actions.push_back(ActionSpec {
                .element = on, .event = event, .button = button, .action = std::string(*name) });
            return true;
        }

        static void wire(Surface& surface, uint32_t element, const std::string& instance,
            const ActionBinding& binding)
        {
            Layer* layer = &surface.layer();
            Context* context = &surface.ctx();
            const auto fire = [layer, context, element, instance, action = binding.action,
                                  event = binding.event](glm::vec2 position, glm::vec2 scroll) {
                action(ActionEvent { *layer, *context, element, instance, event, position, scroll });
            };

            switch (binding.event) {
            case PointerEvent::Press:
                context->subscribe_press(element, binding.button,
                    [fire](uint32_t, glm::vec2 ndc) { fire(ndc, glm::vec2(0.F)); });
                break;
            case PointerEvent::Release:
                context->subscribe_release(element, binding.button,
                    [fire](uint32_t, glm::vec2 ndc) { fire(ndc, glm::vec2(0.F)); });
                break;
            case PointerEvent::Drag:
                context->subscribe_drag(element, binding.button,
                    [fire](uint32_t, glm::vec2 ndc) { fire(ndc, glm::vec2(0.F)); });
                break;
            case PointerEvent::Move:
                context->subscribe_move(element,
                    [fire](uint32_t, glm::vec2 ndc) { fire(ndc, glm::vec2(0.F)); });
                break;
            case PointerEvent::Enter:
                context->subscribe_enter(element,
                    [fire](uint32_t) { fire(glm::vec2(0.F), glm::vec2(0.F)); });
                break;
            case PointerEvent::Leave:
                context->subscribe_leave(element,
                    [fire](uint32_t) { fire(glm::vec2(0.F), glm::vec2(0.F)); });
                break;
            case PointerEvent::Scroll:
                context->subscribe_scroll(element,
                    [fire](uint32_t, glm::vec2 ndc, double dx, double dy) {
                        fire(ndc, glm::vec2(static_cast<float>(dx), static_cast<float>(dy)));
                    });
                break;
            }
        }

        bool check_instances()
        {
            for (SurfaceInstances& entry : m_surfaces) {
                for (const Source& instance : entry.instances) {
                    const auto id = claim_id(instance);
                    if (!id)
                        return false;
                    const auto reference = Reflect::check_reference<Source, Access>(instance,
                        reserved_template_attribute,
                        [this](std::string_view value) { return m_templates.contains(std::string(value)); },
                        m_error);
                    if (!reference)
                        return false;

                    InstanceInfo info { .id = *id, .template_id = std::string(*reference), .layer = {}, .actions = {} };
                    if (!read_layer_attributes(instance, info.layer) || !read_actions(instance, info))
                        return false;
                    entry.info.push_back(std::move(info));
                }
                if (!check_relations(entry))
                    return false;
            }
            return true;
        }

        bool check_relations(const SurfaceInstances& entry)
        {
            for (size_t i = 0; i < entry.info.size(); ++i) {
                const auto& relate = entry.info[i].layer.relate;
                if (!relate)
                    continue;
                if (*relate == entry.info[i].id) {
                    return fail(entry.instances[i], std::string(reserved_prefix)
                            + "relate may not name the instance itself");
                }
                const bool known = std::ranges::any_of(entry.info, [&](const InstanceInfo& other) {
                    return other.id == *relate;
                });
                if (!known) {
                    return fail(entry.instances[i], std::string(reserved_prefix) + "relate refers to "
                            + *relate + ", which is not an instance of surface " + entry.id);
                }
            }
            return true;
        }
    };

}

namespace Detail {

    template <typename Source, typename Access>
    std::optional<PlacedInstances> instantiate(const FactoryTable<Source, Access>& table,
        const ActionTable<Source, Access>* actions, const Source& root,
        SurfaceMap& surfaces, std::string& error)
    {
        error.clear();
        DocumentWalk<Source, Access> walk(table, root, error, actions);
        if (!walk.read_structure())
            return std::nullopt;
        const auto prepared = walk.prepare(surfaces);
        if (!prepared)
            return std::nullopt;

        PlacedInstances placed;
        std::vector<uint32_t> elements;
        elements.reserve(prepared->size());
        for (const auto& item : *prepared) {
            const auto element = item.placer(*item.surface, error);
            if (!element)
                return std::nullopt;
            elements.push_back(*element);
            placed.emplace(item.id, PlacedInstance { .surface = item.surface_id, .element = *element });
        }
        if (!DocumentWalk<Source, Access>::apply(*prepared, elements, error))
            return std::nullopt;
        return placed;
    }

}

/**
 * @brief Place every instance of a document through a table.
 *
 * The document is a tree of a root element named forma carrying a version of 1.
 * The root may also carry xmlns declarations, which are ignored, so the mf:
 * prefix below can be declared for tools that validate namespaces.
 * Its templates element holds template elements, each with an id and exactly one
 * child, the factory, whose name is looked up in the table. Its surface
 * elements hold instance elements with an id, a template naming a template, and
 * the placement attributes the factory describes. Ids are unique across the
 * document. A surface id is a name the host binds in @p surfaces.
 *
 * An instance may also carry attributes of the document's own, all beginning
 * with mf: and applied to the placed element through its Layer:
 * - mf:hidden and mf:interactive take true or false.
 * - mf:relate names another instance of the same surface that this one belongs
 *   with, so hiding, moving or removing that one reaches this one.
 * - mf:front and mf:back move the element, and its relations, to the top or
 *   the bottom of the paint and hit order. They exclude each other.
 * - mf:clip takes x y width height and confines hit testing to that region.
 * Every placed element is also named after its instance, so Layer::find
 * reaches it by the id the document gave it.
 *
 * Placement is two phases. The structure is checked and every instance is
 * decoded first, so a bad document fails before any Surface is touched. Then
 * each prepared instance is placed in document order and the Layer attributes
 * are applied.
 *
 * @tparam Source Element type of the document tree, with a Reflect::RecordAccess.
 * @param table Factories by name.
 * @param root Root element of the document.
 * @param surfaces Surfaces by the name the document's surface elements use.
 * @param error Receives the reason, with a row, when nullopt is returned.
 * @return Each placed instance by id, with its surface and element.
 * @note A failure while placing may leave earlier instances on their Surface.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] std::optional<PlacedInstances> instantiate_document(
    const FactoryTable<Source, Access>& table, const Source& root,
    SurfaceMap& surfaces, std::string& error)
{
    return Detail::instantiate<Source, Access>(table, nullptr, root, surfaces, error);
}

/**
 * @brief Place a document whose instances attach host actions to events.
 *
 * As the overload without actions, and an instance may also hold on children.
 * Each names an event (press, release, drag, move, enter, leave or scroll), a
 * button for press, release and drag (left by default), and an action
 * registered in @p actions, with that action's own parameters as further
 * attributes. The names and parameters are resolved with the rest of the
 * document, so an unknown action fails before anything is placed. The actions
 * are subscribed beside the element's own wiring, so they never replace it.
 *
 * @param actions Actions by name.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] std::optional<PlacedInstances> instantiate_document(
    const FactoryTable<Source, Access>& table, const ActionTable<Source, Access>& actions,
    const Source& root, SurfaceMap& surfaces, std::string& error)
{
    return Detail::instantiate<Source, Access>(table, &actions, root, surfaces, error);
}

/**
 * @brief Make one element per row of typed placements from a single template.
 *
 * The template's factory must have been registered with expose_typed. Its
 * inputs are decoded once and each row, laid out as the described composite
 * layout of the factory's placement type, becomes one element on @p surface.
 * Everything is prepared before anything is placed. Rows carry no ids, so the
 * elements are not named.
 *
 * @param template_id Id of a template in the document.
 * @param rows One row of placement values per element.
 * @return The element id of each row in row order, or nullopt with the reason in error.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] std::optional<std::vector<uint32_t>> instantiate_rows(
    const FactoryTable<Source, Access>& table, const Source& root,
    std::string_view template_id, const Kakshya::CompositeArray& rows,
    Surface& surface, std::string& error)
{
    error.clear();
    Detail::DocumentWalk<Source, Access> walk(table, root, error);
    if (!walk.read_structure())
        return std::nullopt;
    const auto placers = walk.prepare_bulk(template_id, rows);
    if (!placers)
        return std::nullopt;

    std::vector<uint32_t> elements;
    elements.reserve(placers->size());
    for (const auto& placer : *placers) {
        const auto element = placer(surface, error);
        if (!element)
            return std::nullopt;
        elements.push_back(*element);
    }
    return elements;
}

}
