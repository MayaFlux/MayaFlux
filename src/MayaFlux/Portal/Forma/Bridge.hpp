#pragma once

#include "Primitives/Mapped.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
}

namespace MayaFlux::Buffers {
class BufferManager;
class ShaderProcessor;
class AudioWriteProcessor;
class GeometryWriteProcessor;
class FormaBindingsProcessor;
}

namespace MayaFlux::Nodes {
class Node;
class Constant;
}

namespace MayaFlux::Portal::Forma {

/**
 * @class Bridge
 * @brief Two-way binding orchestrator for Forma elements.
 *
 * One Bridge instance serves the full application. It owns all inbound
 * GraphicsRoutine tasks and all outbound FormaBindingsProcessor instances.
 * Layer and Context are created via the Forma free functions and passed in
 * — Bridge does not own or create them.
 *
 * Inbound (what drives an element's MappedState each frame):
 *   - bind(id, Node)     — reads Node::get_last_output() via GraphicsRoutine
 *   - bind(id, lambda)   — calls std::function<float()> via GraphicsRoutine
 *
 * Outbound (where the element's value goes each frame):
 *   - write(id, ShaderProcessor, offset) — push constant staging
 *   - write(id, descriptor_name, ...)    — descriptor binding
 *   - write(id, AudioWriteProcessor)     — audio buffer
 *   - write(id, GeometryWriteProcessor)  — vertex buffer
 *   - write(id, Constant)                — node graph value carrier
 *
 * All overloads also accept shared_ptr<MappedState<T>> in place of the
 * element id. These resolve to the state->id.
 * duplicated logic.
 *
 * Usage:
 * @code
 * auto [layer, ctx] = Forma::create_layer(window, "hud", event_manager);
 * auto el = Forma::create_element<float>(*layer, window, bm, geom, 0.5f);
 *
 * Bridge bridge(scheduler, buffer_manager);
 * bridge.bind(el.state, envelope);
 * bridge.write(el.state, compute_proc, offsetof(PC, cutoff));
 *
 * ctx->on_press(el.element.id, IO::MouseButtons::LEFT,
 *     [](uint32_t, glm::vec2){});
 * @endcode
 */
class MAYAFLUX_API Bridge {
public:
    Bridge(
        Vruta::TaskScheduler& scheduler,
        Buffers::BufferManager& buffer_manager);

    ~Bridge();

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;
    Bridge(Bridge&&) = delete;
    Bridge& operator=(Bridge&&) = delete;

    // =========================================================================
    // Inbound — id overloads (primary implementation)
    // =========================================================================

    /**
     * @brief Drive element value from a Node's output each frame.
     *
     * Spawns a GraphicsRoutine that reads node->get_last_output() every tick,
     * applies @p project, and writes into the element's MappedState.
     * Node must be processed externally (EXTERNAL mode read).
     * Replaces any existing inbound binding on this element.
     *
     * @param id      Element id (from Layer::add / Mapped::element.id).
     * @param node    Node to read from.
     * @param project Optional double -> float projection. Identity if empty.
     */
    void bind(uint32_t id,
        std::shared_ptr<Nodes::Node> node,
        std::function<float(double)> project = {});

    /**
     * @brief Drive element value from an arbitrary per-frame callable.
     *
     * Spawns a GraphicsRoutine that calls @p source every tick and writes
     * the result into the element's MappedState.
     * Replaces any existing inbound binding on this element.
     *
     * @param id     Element id.
     * @param source Callable returning current float value.
     */
    void bind(uint32_t id, std::function<float()> source);

    // =========================================================================
    // Inbound — MappedState overloads
    // =========================================================================

    template <typename T>
    void bind(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Nodes::Node> node,
        std::function<float(double)> project = {})
    {
        bind(state->id, std::move(node), std::move(project));
    }

    template <typename T>
    void bind(std::shared_ptr<MappedState<T>> state, std::function<float()> source)
    {
        bind(state->id, std::move(source));
    }

    // =========================================================================
    // Outbound — id overloads (primary implementation)
    // =========================================================================

    /**
     * @brief Route element value to a push constant slot on a ShaderProcessor.
     * @param id     Element id.
     * @param target VKBuffer whose push_constant staging receives the value.
     * @param offset Byte offset in the push constant struct.
     * @param size   Byte width. Defaults to sizeof(float).
     */
    void write(
        uint32_t id,
        const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
        const std::string& shader_path,
        uint32_t offset,
        size_t size = sizeof(float));

    /**
     * @brief Route element value to a descriptor binding on @p target_buffer.
     *
     * Creates a FormaBindingsProcessor if one does not yet exist for this element
     * and attaches it to @p target_buffer. If a processor already exists from a
     * prior write() call, @p target_buffer must be the same buffer — a second
     * attachment is not made.
     *
     * @param id              Element id.
     * @param target_buffer   Buffer whose pipeline context receives the descriptor update.
     * @param shader_path     Shader path used to construct FormaBindingsProcessor if needed.
     * @param descriptor_name Descriptor name in the shader config.
     * @param binding_index   Vulkan binding index.
     * @param set             Descriptor set index.
     * @param role            UNIFORM or STORAGE.
     */
    void write(uint32_t id,
        const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
        const std::string& shader_path,
        const std::string& descriptor_name,
        uint32_t binding_index,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM);

    /**
     * @brief Route element value to an AudioWriteProcessor each frame.
     * @param id     Element id.
     * @param target AudioWriteProcessor to call set_data() on.
     */
    void write(uint32_t id, std::shared_ptr<Buffers::AudioWriteProcessor> target);

    /**
     * @brief Route element value to a GeometryWriteProcessor each frame.
     * @param id     Element id.
     * @param target GeometryWriteProcessor to call set_data() on.
     */
    void write(uint32_t id, std::shared_ptr<Buffers::GeometryWriteProcessor> target);

    /**
     * @brief Route element value into a Constant node via set_constant().
     * @param id   Element id.
     * @param node Constant node updated each frame.
     */
    void write(uint32_t id, std::shared_ptr<Nodes::Constant> node);

    // =========================================================================
    // Outbound — MappedState overloads
    // =========================================================================

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::VKBuffer> target_buffer,
        const std::string& shader_path,
        uint32_t offset,
        size_t size = sizeof(float))
    {
        write(state->id, std::move(target_buffer), shader_path, offset, size);
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::VKBuffer> target_buffer,
        const std::string& shader_path,
        const std::string& descriptor_name,
        uint32_t binding_index,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM)
    {
        write(state->id, std::move(target_buffer), shader_path,
            descriptor_name, binding_index, set, role);
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::AudioWriteProcessor> target)
    {
        write(state->id, std::move(target));
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::GeometryWriteProcessor> target)
    {
        write(state->id, std::move(target));
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Nodes::Constant> node)
    {
        write(state->id, std::move(node));
    }

    // =========================================================================
    // Registration — called by Forma::create_element to make state findable
    // =========================================================================

    /**
     * @brief Register a MappedState<T> so that MappedState overloads and
     *        outbound bindings can resolve to the correct element id and reader.
     *
     * Called by Forma::create_element. The reader closes over the MappedState
     * and is stored type-erased as std::function<float()>.
     *
     * @tparam T        MappedState value type.
     * @param state     MappedState to register.
     * @param id        Element id from Layer::add.
     * @param buffer    FormaBuffer the element renders into.
     * @param project   Optional T -> float projection. Identity cast if empty.
     */
    template <typename T>
    void register_element(
        std::shared_ptr<MappedState<T>> state,
        uint32_t id,
        std::shared_ptr<Buffers::FormaBuffer> buffer,
        std::function<float(T)> project = {})
    {
        std::function<float()> reader = project
            ? std::function<float()>([s = state, p = std::move(project)] {
                  return p(s->value);
              })
            : std::function<float()>([s = state] {
                  return static_cast<float>(s->value);
              });

        std::function<void(float)> writer = [s = state](float v) {
            s->write(static_cast<T>(v));
        };

        state->id = id;
        m_records[id] = ElementRecord {
            .buffer = std::move(buffer),
            .bindings = nullptr,
            .reader = std::move(reader),
            .writer = std::move(writer),
            .inbound_task = {},
            .outbound_tasks = {},
        };
    }

    /**
     * @brief Register a fully constructed Mapped<T> and own its sync loop.
     *
     * Delegates record and reader/writer setup to the existing
     * register_element overload, then spawns a per-frame GraphicsRoutine
     * that calls mapped.sync(). The routine is stored in outbound_tasks
     * and cancelled by unbind().
     *
     * @p mapped must outlive the Bridge registration. The sync routine
     * captures @p mapped by reference -- the caller is responsible for
     * ensuring the Mapped<T> is not destroyed while the Bridge holds it.
     *
     * @tparam T      MappedState value type.
     * @param mapped  Fully constructed Mapped<T>, typically from create_element.
     * @param project Optional T -> float projection for outbound readers.
     *                Identity cast used if empty.
     */
    template <typename T>
    void register_element(Mapped<T> mapped, std::function<float(T)> project = {})
    {
        register_element(mapped.state, mapped.element.id, mapped.element.buffer, std::move(project));
        spawn_sync(mapped.element.id, [m = std::move(mapped)]() mutable { m.sync(); });
    }

    // =========================================================================
    // Binding lifecycle
    // =========================================================================

    /**
     * @brief Cancel all inbound and outbound bindings for an element.
     *        The element remains in its layer.
     */
    void unbind(uint32_t id);

    template <typename T>
    void unbind(std::shared_ptr<MappedState<T>> state)
    {
        unbind(state->id);
    }

private:
    struct ElementRecord {
        std::shared_ptr<Buffers::FormaBuffer> buffer;
        std::shared_ptr<Buffers::FormaBindingsProcessor> bindings;
        std::function<float()> reader; ///< reads current value from MappedState (outbound)
        std::function<void(float)> writer; ///< writes new value into MappedState (inbound)
        std::string inbound_task;
        std::vector<std::string> outbound_tasks;
    };

    Vruta::TaskScheduler& m_scheduler;
    Buffers::BufferManager& m_buffer_manager;

    mutable uint32_t m_next_id { 0 };

    std::unordered_map<uint32_t, ElementRecord> m_records;

    std::string make_task_name(uint32_t id, const char* suffix) const;
    void cancel_inbound(ElementRecord& rec);
    void cancel_outbound(ElementRecord& rec);
    void spawn_inbound(uint32_t id, std::function<float()> source);

    /**
     * @brief Spawn a per-frame GraphicsRoutine that calls @p sync_fn each tick.
     *
     * Stores the task name in the outbound_tasks of the record for @p id so
     * that unbind() cancels it correctly. The coroutine body is type-free --
     * all type-specific work is captured inside @p sync_fn by the caller.
     *
     * @param id       Element id whose outbound_tasks receives the task name.
     * @param sync_fn  Callable invoked once per frame. Typically a lambda
     *                 capturing Mapped<T> by reference and calling sync().
     */
    void spawn_sync(uint32_t id, std::function<void()> sync_fn);
};

} // namespace MayaFlux::Portal::Forma
