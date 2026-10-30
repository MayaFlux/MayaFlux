#pragma once

#include "MayaFlux/API/Graph.hpp"

/**
 * @file API/Stagecraft.hpp
 * @brief Nexus objects that need the engine's managers, built with them already supplied.
 *
 * Fabric and Tapestry take the scheduler, event manager and buffer manager as
 * explicit arguments. These functions pass the default engine's. Everything
 * else in Nexus takes no manager and is constructed directly.
 *
 * A Fabric keeps references to the engine's managers, so release it before
 * MayaFlux::End(), or hand it to store() to have the persistent store do so.
 */

namespace MayaFlux {

namespace Core {
    class Window;
}

namespace Nexus {
    class Fabric;
    class Tapestry;
}

namespace Portal::Graphics {
    struct RenderConfig;
}

/**
 * @brief Construct a named Fabric over the default engine's managers.
 * @param name      Name used when the Fabric is encoded.
 * @param cell_size Grid cell edge length for the spatial index.
 *
 * @code
 * auto fabric = MayaFlux::create_fabric("main");
 * fabric->wire(emitter).every(0.1).finalise();
 * @endcode
 */
MAYAFLUX_API std::shared_ptr<Nexus::Fabric> create_fabric(const std::string& name, float cell_size = 1.0F);

/**
 * @brief Construct a Tapestry over the default engine's managers.
 *
 * Every Fabric the Tapestry creates is built over the same managers.
 *
 * @code
 * auto tapestry = MayaFlux::create_tapestry();
 * auto fabric = tapestry->create_fabric("main");
 * @endcode
 */
MAYAFLUX_API std::shared_ptr<Nexus::Tapestry> create_tapestry();

/**
 * @brief Register an audio output for an Agent or Emitter on @p channel.
 *
 * Forwards to the entity's sink_audio with the engine's buffer manager.
 * @p args are the optional producer function and name.
 */
template <typename Entity, typename... Args>
    requires requires(Entity& entity, Buffers::BufferManager& manager, uint32_t channel, Args&&... args) {
        entity.sink_audio(manager, channel, std::forward<Args>(args)...);
    }
void sink_audio(const std::shared_ptr<Entity>& entity, uint32_t channel, Args&&... args)
{
    entity->sink_audio(*get_buffer_manager(), channel, std::forward<Args>(args)...);
}

/**
 * @brief Register a render output for an Agent or Emitter.
 *
 * Forwards to the entity's render with the engine's buffer manager.
 * @p args are the optional name and producer function.
 */
template <typename Entity, typename... Args>
    requires requires(Entity& entity, Buffers::BufferManager& manager, const Portal::Graphics::RenderConfig& config, Args&&... args) {
        entity.render(manager, config, std::forward<Args>(args)...);
    }
void sink_render(const std::shared_ptr<Entity>& entity, const Portal::Graphics::RenderConfig& config, Args&&... args)
{
    entity->render(*get_buffer_manager(), config, std::forward<Args>(args)...);
}

/**
 * @brief Unregister the audio output of an Agent or Emitter on @p channel.
 *
 * Needed only for an entity outside a Fabric; a Fabric releases the sinks of
 * the entities it removes.
 */
template <typename Entity>
    requires requires(Entity& entity, Buffers::BufferManager& manager, uint32_t channel) {
        entity.remove_audio_sink(manager, channel);
    }
void remove_audio_sink(const std::shared_ptr<Entity>& entity, uint32_t channel)
{
    entity->remove_audio_sink(*get_buffer_manager(), channel);
}

/**
 * @brief Unregister the render output of an Agent or Emitter targeting @p window.
 *
 * Needed only for an entity outside a Fabric.
 */
template <typename Entity>
    requires requires(Entity& entity, Buffers::BufferManager& manager, const std::shared_ptr<Core::Window>& window) {
        entity.remove_render(manager, window);
    }
void remove_render_sink(const std::shared_ptr<Entity>& entity, const std::shared_ptr<Core::Window>& window)
{
    entity->remove_render(*get_buffer_manager(), window);
}

} // namespace MayaFlux
