#pragma once

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @brief Settle a vertex buffer's render config for the spans it will draw.
 * @param requested What the caller asked for.
 * @param run_topologies Topology of every span the buffer will draw, or none when its
 *        spans are not known yet.
 * @return The config to apply: a point topology becomes the first non point span topology,
 *         triangulation is switched on when the topology or any span needs it, and every
 *         shader the caller left empty takes the default for the topology.
 */
[[nodiscard]] MAYAFLUX_API Portal::Graphics::RenderConfig resolve_geometry_render_config(
    const Portal::Graphics::RenderConfig& requested,
    std::span<const Portal::Graphics::PrimitiveTopology> run_topologies);

/**
 * @brief Make a triangulating point mill draw its whole range instead of the vertex extent.
 * @param render Processor whose mill is adjusted. Null is ignored.
 * @param resolved Config returned by resolve_geometry_render_config.
 *
 * Only a triangulating render of points as quads is touched.
 */
MAYAFLUX_API void tune_point_mill(
    const std::shared_ptr<RenderProcessor>& render,
    const Portal::Graphics::RenderConfig& resolved);

/**
 * @brief Hand a render processor the spans to draw, when they differ from the last ones.
 * @param render Processor to update. Null is ignored.
 * @param config The buffer's applied render config.
 * @param applied_runs Spans last handed over, replaced by @p runs when they differ.
 * @param runs Spans in the buffer's vertex coordinates.
 *
 * A triangulating point mill draws line spans as points.
 */
MAYAFLUX_API void apply_draw_runs(
    const std::shared_ptr<RenderProcessor>& render,
    const Portal::Graphics::RenderConfig& config,
    std::vector<Portal::Graphics::DrawRun>& applied_runs,
    std::vector<Portal::Graphics::DrawRun> runs);

} // namespace MayaFlux::Buffers
