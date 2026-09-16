#include "Inspector.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma {

InspectResult Inspector::event(
    const std::shared_ptr<Vruta::Event>& ev,
    std::string_view name,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::string header_label = name.empty() ? "(unnamed)" : std::string(name);

    std::vector<EntrySpec> entrys {
        EntrySpec {
            .label = "token",
            .reader = [ev] {
                return std::string(Reflect::enum_to_string(ev->get_processing_token()));
            },
        },
        EntrySpec {
            .label = "active",
            .reader = [ev] { return ev->is_active() ? "true" : "false"; },
        },
    };

    const auto dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
    auto hbuf = make_header_buffer(surface.window());
    std::vector<EntryBuffer> rbufs;
    rbufs.reserve(entrys.size());
    for (const auto& spec : entrys)
        rbufs.push_back(make_row_buffer(surface.window(), spec.label, dims));

    auto group = make_entry_group(entrys, header_label, std::move(hbuf), rbufs,
        surface, cursor, x_min, x_max, row_h, false);

    InspectResult result;
    result.group = std::move(group);
    return result;
}

InspectResult& Inspector::event_manager(
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    const std::vector<EntrySpec> root_entrys {
        EntrySpec {
            .label = "events",
            .reader = [&m_event_mgr = m_event_mgr] {
                return std::to_string(m_event_mgr.get_all_events().size());
            },
        },
    };

    const auto dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
    auto hbuf = make_header_buffer(surface.window());
    std::vector<EntryBuffer> rbufs;
    rbufs.reserve(root_entrys.size());
    for (const auto& spec : root_entrys)
        rbufs.push_back(make_row_buffer(surface.window(), spec.label, dims));

    auto root_group = make_entry_group(root_entrys, "EventManager", std::move(hbuf), rbufs,
        surface, cursor, x_min, x_max, row_h, true);

    InspectResult& result = s_event_result.emplace();
    result.group = std::move(root_group);

    const auto names = m_event_mgr.get_event_names();

    for (const auto& ev : m_event_mgr.get_all_events()) {
        if (!ev)
            continue;

        std::string header_label = "(unnamed)";
        for (const auto& n : names) {
            if (m_event_mgr.get_event(n) == ev) {
                header_label = n;
                break;
            }
        }

        std::vector<EntrySpec> entrys {
            EntrySpec {
                .label = "active",
                .reader = [ev] { return ev->is_active() ? "true" : "false"; },
            },
        };

        const auto ev_dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
        auto ev_hbuf = make_header_buffer(surface.window());
        std::vector<EntryBuffer> ev_rbufs;
        ev_rbufs.reserve(entrys.size());
        for (const auto& spec : entrys)
            ev_rbufs.push_back(make_row_buffer(surface.window(), spec.label, ev_dims));

        auto ev_group = make_entry_group(entrys, header_label, std::move(ev_hbuf), ev_rbufs,
            surface, cursor, x_min, x_max, row_h, false);

        InspectResult ev_result;
        ev_result.group = std::move(ev_group);

        result.group.header.attach(surface.layer(), ev_result.group.header.header_id);
        result.children.push_back(std::move(ev_result));
    }

    return result;
}

} // namespace MayaFlux::Portal::Forma
