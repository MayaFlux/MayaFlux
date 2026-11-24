#include "GeometryRenderSetup.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

namespace MayaFlux::Buffers {

namespace {

    using Portal::Graphics::PrimitiveTopology;
    using Portal::Graphics::RenderConfig;

    void configure_shader_defaults(RenderConfig& resolved, const RenderConfig& requested)
    {
        if (resolved.triangulate)
            resolved.geometry_shader.clear();

        switch (resolved.topology) {
        case PrimitiveTopology::POINT_LIST:
            if (requested.vertex_shader.empty())
                resolved.vertex_shader = "point.vert.spv";
            if (requested.fragment_shader.empty())
                resolved.fragment_shader = "point.frag.spv";
            break;
        case PrimitiveTopology::LINE_LIST:
        case PrimitiveTopology::LINE_STRIP:
            if (requested.vertex_shader.empty())
                resolved.vertex_shader = "line.vert.spv";
            if (requested.fragment_shader.empty())
                resolved.fragment_shader = "line.frag.spv";
            if (!resolved.triangulate && requested.geometry_shader.empty())
                resolved.geometry_shader = "line.geom.spv";
            break;
        case PrimitiveTopology::TRIANGLE_LIST:
        case PrimitiveTopology::TRIANGLE_STRIP:
        case PrimitiveTopology::TRIANGLE_FAN:
            if (requested.vertex_shader.empty())
                resolved.vertex_shader = "triangle.vert.spv";
            if (requested.fragment_shader.empty())
                resolved.fragment_shader = "triangle.frag.spv";
            break;
        default:
            if (requested.vertex_shader.empty())
                resolved.vertex_shader = "point.vert.spv";
            if (requested.fragment_shader.empty())
                resolved.fragment_shader = "point.frag.spv";
        }
    }

} // namespace

RenderConfig resolve_geometry_render_config(
    const RenderConfig& requested,
    std::span<const PrimitiveTopology> run_topologies)
{
    auto resolved = requested;

    if (resolved.topology == PrimitiveTopology::POINT_LIST) {
        const auto first_non_point = std::ranges::find_if(
            run_topologies, [](auto topology) { return topology != PrimitiveTopology::POINT_LIST; });
        if (first_non_point != run_topologies.end())
            resolved.topology = *first_non_point;
    }

    resolved.triangulate |= Portal::Graphics::needs_triangulation(
        resolved.topology, resolved.polygon_mode);
    resolved.triangulate |= std::ranges::any_of(run_topologies,
        [&](auto topology) {
            return Portal::Graphics::needs_triangulation(topology, resolved.polygon_mode);
        });
    configure_shader_defaults(resolved, requested);

    return resolved;
}

void tune_point_mill(
    const std::shared_ptr<RenderProcessor>& render,
    const RenderConfig& resolved)
{
    if (!render) {
        return;
    }

    if (resolved.triangulate
        && resolved.polygon_mode == Portal::Graphics::PolygonMode::POINT) {
        auto mill_spec = render->get_mill_spec();
        mill_spec.use_vertex_extent = false;
        render->set_mill_spec(mill_spec);
    }
}

void apply_draw_runs(
    const std::shared_ptr<RenderProcessor>& render,
    const RenderConfig& config,
    std::vector<Portal::Graphics::DrawRun>& applied_runs,
    std::vector<Portal::Graphics::DrawRun> runs)
{
    if (!render) {
        return;
    }

    const bool point_mill = config.triangulate
        && config.polygon_mode == Portal::Graphics::PolygonMode::POINT;
    if (point_mill) {
        render->set_triangulate(true);
    }

    const bool unchanged = std::ranges::equal(runs, applied_runs,
        [](const auto& a, const auto& b) {
            return a.topology == b.topology && a.vertex_offset == b.vertex_offset
                && a.vertex_count == b.vertex_count && a.fill == b.fill;
        });
    if (unchanged) {
        return;
    }
    applied_runs = runs;

    if (point_mill) {
        for (auto& run : runs) {
            if (run.topology == PrimitiveTopology::LINE_LIST
                || run.topology == PrimitiveTopology::LINE_STRIP) {
                run.topology = PrimitiveTopology::POINT_LIST;
            }
        }
    }
    render->set_runs(std::move(runs));
}

} // namespace MayaFlux::Buffers
