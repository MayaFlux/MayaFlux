#pragma once

#include "FactoryTable.hpp"

namespace MayaFlux::Portal::Forma {

/** @brief Surfaces by the document name a host binds them to. */
using SurfaceMap = std::map<std::string, Surface, std::less<>>;

namespace Detail {

    template <typename Source, typename Access>
    class DocumentWalk {
    public:
        using Table = FactoryTable<Source, Access>;
        using Placer = typename Table::Placer;
        using Prepared = std::vector<std::pair<Surface*, Placer>>;

        DocumentWalk(const Table& table, const Source& root, std::string& error)
            : m_table(table)
            , m_root(root)
            , m_error(error)
        {
        }

        bool read_structure()
        {
            if (Access::name(m_root) != "forma")
                return fail(m_root, "root element must be <forma>");
            if (!only_attributes(m_root, { "version" }) || !check_version())
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
                for (const Source& instance : entry.instances) {
                    const std::string reference(*Access::attribute(instance, reserved_template_attribute));
                    const Source& factory_element = m_templates.at(reference);
                    const typename Table::Instantiator* decode = m_table.find(Access::name(factory_element));
                    auto placer = std::invoke(*decode, factory_element, instance, m_error);
                    if (!placer) {
                        if (m_error.empty())
                            fail(instance, "could not prepare the instance");
                        return std::nullopt;
                    }
                    prepared.emplace_back(&bound->second, std::move(*placer));
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

    private:
        struct SurfaceInstances {
            std::string id;
            Source element;
            std::vector<Source> instances;
        };

        const Table& m_table;
        const Source& m_root;
        std::string& m_error;
        std::unordered_map<std::string, uint64_t> m_ids;
        std::unordered_map<std::string, Source> m_templates;
        std::vector<SurfaceInstances> m_surfaces;

        bool fail(const Source& at, std::string message)
        {
            m_error = Table::located(Access::location(at), message);
            return false;
        }

        bool only_attributes(const Source& source, std::initializer_list<std::string_view> allowed)
        {
            return Access::for_each_attribute(source, [&](std::string_view name, std::string_view) {
                if (std::ranges::find(allowed, name) != allowed.end())
                    return true;
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
            const auto id = Access::attribute(source, reserved_id_attribute);
            if (!id || id->empty()) {
                fail(source, "<" + std::string(Access::name(source)) + "> needs a non-empty id");
                return std::nullopt;
            }
            const auto [slot, inserted] = m_ids.emplace(std::string(*id), Access::location(source));
            if (!inserted) {
                fail(source, "id " + std::string(*id) + " is already used at row "
                        + std::to_string(slot->second));
                return std::nullopt;
            }
            return std::string(*id);
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

            SurfaceInstances entry { .id = *id, .element = surface, .instances = {} };
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

        bool check_instances()
        {
            for (const SurfaceInstances& entry : m_surfaces) {
                for (const Source& instance : entry.instances) {
                    if (!claim_id(instance))
                        return false;
                    const auto reference = Access::attribute(instance, reserved_template_attribute);
                    if (!reference)
                        return fail(instance, "<instance> needs a template attribute");
                    if (!m_templates.contains(std::string(*reference)))
                        return fail(instance, "<instance> refers to unknown template " + std::string(*reference));
                }
            }
            return true;
        }
    };

}

/**
 * @brief Place every instance of a document through a table.
 *
 * The document is a tree of a root element named forma carrying a version of 1.
 * Its templates element holds template elements, each with an id and exactly one
 * child, the factory, whose name is looked up in the table. Its surface
 * elements hold instance elements with an id, a template naming a template, and
 * the placement attributes the factory describes. Ids are unique across the
 * document. A surface id is a name the host binds in @p surfaces.
 *
 * Placement is two phases. The structure is checked and every instance is
 * decoded first, so a bad document fails before any Surface is touched. Then
 * each prepared instance is placed in document order.
 *
 * @tparam Source Element type of the document tree, with a Reflect::RecordAccess.
 * @param table Factories by name.
 * @param root Root element of the document.
 * @param surfaces Surfaces by the name the document's surface elements use.
 * @param error Receives the reason, with a row, when nullopt is returned.
 * @return The number of instances placed.
 * @note A failure while placing may leave earlier instances on their Surface.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] std::optional<size_t> instantiate_document(
    const FactoryTable<Source, Access>& table, const Source& root,
    SurfaceMap& surfaces, std::string& error)
{
    error.clear();
    Detail::DocumentWalk<Source, Access> walk(table, root, error);
    if (!walk.read_structure())
        return std::nullopt;
    auto prepared = walk.prepare(surfaces);
    if (!prepared)
        return std::nullopt;

    size_t count = 0;
    for (const auto& [surface, placer] : *prepared) {
        if (!placer(*surface, error))
            return std::nullopt;
        ++count;
    }
    return count;
}

/**
 * @brief Make one element per row of typed placements from a single template.
 *
 * The template's factory must have been registered with expose_typed. Its
 * inputs are decoded once and each row, laid out as the described composite
 * layout of the factory's placement type, becomes one element on @p surface.
 * Everything is prepared before anything is placed.
 *
 * @param template_id Id of a template in the document.
 * @param rows One row of placement values per element.
 * @return The number placed, or nullopt with the reason in error.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] std::optional<size_t> instantiate_rows(
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

    size_t count = 0;
    for (const auto& placer : *placers) {
        if (!placer(surface, error))
            return std::nullopt;
        ++count;
    }
    return count;
}

}
