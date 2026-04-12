#pragma once

#include "InfluenceContext.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Graphics {
struct RenderConfig;
}

namespace MayaFlux::Buffers {
class AudioBuffer;
class VKBuffer;
class BufferManager;
class AudioWriteProcessor;
class GeometryWriteProcessor;
class TextureWriteProcessor;
class RenderProcessor;
}

namespace MayaFlux::Nexus {

using RenderFn = std::function<void(const InfluenceContext&)>;

/**
 * @struct AudioSink
 * @brief Holds the plumbing for one audio output registered from a Nexus object.
 *
 * Created by add_audio_sink(). Owns the AudioBuffer and AudioWriteProcessor.
 * The buffer is registered with the supplied BufferManager on construction
 * and unregistered on destruction via remove_audio_sink().
 *
 * If @c fn is set, dispatch_audio_sinks() calls it with the current
 * InfluenceContext and forwards the result to the writer. If @c fn is empty,
 * data arrives only via push_audio_data().
 */
struct AudioSink {
    std::shared_ptr<Buffers::AudioBuffer> buf;
    std::shared_ptr<Buffers::AudioWriteProcessor> writer;
    std::function<Kakshya::DataVariant(const InfluenceContext&)> fn;
    uint32_t channel {};
};

/**
 * @struct RenderSink
 * @brief Holds the plumbing for one graphics output registered from a Nexus object.
 *
 * Created by add_render_sink(). Owns the VKBuffer, GeometryWriteProcessor,
 * and RenderProcessor. The buffer is registered with the supplied BufferManager
 * on construction and unregistered on destruction via remove_render_sink().
 *
 * Default rendering uses POINT_LIST topology with point.vert.spv /
 * point.frag.spv. The object's spatial position is written as a single
 * point when dispatch_render_sinks() fires and no fn is set.
 *
 * If @c fn is set, dispatch_render_sinks() calls it and forwards the result
 * to the writer. If @c fn is empty, data arrives only via push_geometry().
 */
struct RenderSink {
    std::shared_ptr<Buffers::VKBuffer> buf;
    std::shared_ptr<Buffers::GeometryWriteProcessor> writer;
    std::shared_ptr<Buffers::RenderProcessor> renderer;
    RenderFn fn;
    std::shared_ptr<Core::Window> window;
};

// =============================================================================
// Audio sink free functions
// =============================================================================

/**
 * @brief Create and register an audio sink on @p channel.
 * @param sinks   Sink vector owned by the calling Emitter or Agent.
 * @param mgr     BufferManager to register the AudioBuffer with.
 * @param channel Output channel index.
 * @param fn      Optional producer called each dispatch with InfluenceContext.
 *                Leave empty when data is supplied via push_audio_data().
 */
void add_audio_sink(
    std::vector<AudioSink>& sinks,
    Buffers::BufferManager& mgr,
    uint32_t channel,
    std::function<Kakshya::DataVariant(const InfluenceContext&)> fn = {});

/**
 * @brief Unregister and destroy the audio sink on @p channel.
 * @param sinks   Sink vector owned by the calling Emitter or Agent.
 * @param mgr     BufferManager the AudioBuffer was registered with.
 * @param channel Channel index passed to add_audio_sink().
 */
void remove_audio_sink(
    std::vector<AudioSink>& sinks,
    Buffers::BufferManager& mgr,
    uint32_t channel);

/**
 * @brief Push @p samples to every audio sink in @p sinks.
 * @param sinks   Sink vector owned by the calling Emitter or Agent.
 * @param samples Sample data forwarded to AudioWriteProcessor::set_data().
 */
void push_audio_data(
    std::vector<AudioSink>& sinks,
    std::span<const double> samples);

/**
 * @brief For each sink that has a producer fn, call it and push the result.
 *        Sinks without a fn are untouched.
 * @param sinks Sink vector owned by the calling Emitter or Agent.
 * @param ctx   Current InfluenceContext passed to each fn.
 */
void dispatch_audio_sinks(
    std::vector<AudioSink>& sinks,
    const InfluenceContext& ctx);

// =============================================================================
// Render sink free functions
// =============================================================================

/**
 * @brief Create and register a render sink targeting @p window.
 *
 * Allocates a VKBuffer (VERTEX, POINT_LIST), attaches a GeometryWriteProcessor
 * and a RenderProcessor with point.vert.spv / point.frag.spv defaults, and
 * registers the buffer with @p mgr.
 *
 * @param sinks  Sink vector owned by the calling Emitter or Agent.
 * @param mgr    BufferManager to register the VKBuffer with.
 * @param window Target window for presentation.
 * @param fn     Optional producer called each dispatch with InfluenceContext.
 *               Leave empty when geometry is supplied via push_geometry().
 * @param initial_position Optional initial position to write if no producer fn is set.
 */
void add_render_sink(
    std::vector<RenderSink>& sinks,
    Buffers::BufferManager& mgr,
    const Portal::Graphics::RenderConfig& config,
    RenderFn fn = {},
    const std::optional<glm::vec3>& initial_position = {});

/**
 * @brief Unregister and destroy the render sink targeting @p window.
 * @param sinks  Sink vector owned by the calling Emitter or Agent.
 * @param mgr    BufferManager the VKBuffer was registered with.
 * @param window Window passed to add_render_sink().
 */
void remove_render_sink(
    std::vector<RenderSink>& sinks,
    Buffers::BufferManager& mgr,
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Push pre-resolved vertex bytes to every render sink.
 * @param sinks      Sink vector owned by the calling Emitter or Agent.
 * @param data       Pointer to vertex data.
 * @param byte_count Total size in bytes.
 * @param layout     VertexLayout describing stride and attributes.
 */
void push_vertices(
    std::vector<RenderSink>& sinks,
    const void* data, size_t byte_count,
    const Kakshya::VertexLayout& layout);

/**
 * @brief For each sink that has a producer fn, call it and push the result.
 *        If no fn is set, writes the object position as a single point.
 * @param sinks Sink vector owned by the calling Emitter or Agent.
 * @param ctx   Current InfluenceContext passed to each fn.
 */
void dispatch_render_sinks(
    std::vector<RenderSink>& sinks,
    const InfluenceContext& ctx);

} // namespace MayaFlux::Nexus
