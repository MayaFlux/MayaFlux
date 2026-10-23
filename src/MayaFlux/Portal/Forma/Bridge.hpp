#pragma once

#include "Primitives/Mapped.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

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
class DataWriteProcessor;
class FormaBindingsProcessor;
}

namespace MayaFlux::Nodes {
class Node;
class Constant;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief Layer-scoped element identity. Element ids are unique only within
 *        their Layer, so the owning Layer is half of the key.
 */
using ElementRef = std::pair<std::weak_ptr<Layer>, uint32_t>;

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
 *   - bind(ref, Node)     — reads Node::get_last_output() via GraphicsRoutine
 *   - bind(ref, lambda)   — calls std::function<float()> via GraphicsRoutine
 *
 * Outbound (where the element's value goes each frame):
 *   - write(ref, ShaderProcessor, offset) — push constant staging
 *   - write(ref, descriptor_name, ...)    — descriptor binding
 *   - write(ref, AudioWriteProcessor)     — audio buffer
 *   - write(ref, DataWriteProcessor)      — vertex buffer
 *   - write(ref, Constant)                — node graph value carrier
 *
 * Element ids are unique only within their Layer, so every element is
 * addressed by an ElementRef. All overloads also accept
 * shared_ptr<MappedState<T>> in place of the ElementRef; the state carries
 * both halves once registered.
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
    // Inbound — ElementRef overloads (primary implementation)
    // =========================================================================

    /**
     * @brief Drive element value from a Node's output each frame.
     *
     * Spawns a GraphicsRoutine that reads node->get_last_output() every tick,
     * applies @p project, and writes into the element's MappedState.
     * Node must be processed externally (EXTERNAL mode read).
     * Replaces any existing inbound binding on this element.
     *
     * @param ref     Owning Layer and element id (from Layer::add / Mapped::element.id).
     * @param node    Node to read from.
     * @param project Optional double -> float projection. Identity if empty.
     */
    void bind(const ElementRef& ref,
        std::shared_ptr<Nodes::Node> node,
        std::function<float(double)> project = {});

    /**
     * @brief Drive element value from an arbitrary per-frame callable.
     *
     * Spawns a GraphicsRoutine that calls @p source every tick and writes
     * the result into the element's MappedState.
     * Replaces any existing inbound binding on this element.
     *
     * @param ref    Owning Layer and element id.
     * @param source Callable returning current float value.
     */
    void bind(const ElementRef& ref, std::function<float()> source);

    // =========================================================================
    // Inbound — MappedState overloads
    // =========================================================================

    template <typename T>
    void bind(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Nodes::Node> node,
        std::function<float(double)> project = {})
    {
        bind(ref_of(state), std::move(node), std::move(project));
    }

    template <typename T>
    void bind(std::shared_ptr<MappedState<T>> state, std::function<float()> source)
    {
        bind(ref_of(state), std::move(source));
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::function<void(std::span<const float>)> sink)
    {
        write(ref_of(state), std::move(sink));
    }

    // =========================================================================
    // Outbound — ElementRef overloads (primary implementation)
    // =========================================================================

    /**
     * @brief Route element value to a push constant slot on a ShaderProcessor.
     * @param ref    Owning Layer and element id.
     * @param target VKBuffer whose push_constant staging receives the value.
     * @param offset Byte offset in the push constant struct.
     * @param size   Byte width. Defaults to sizeof(float).
     */
    void write(
        const ElementRef& ref,
        const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
        const std::string& shader_path,
        uint32_t offset,
        size_t size = sizeof(float));

    /**
     * @brief Route element value to a push constant slot described by a
     *        GpuDataFormat.
     *
     * The element value is converted to the format; integers saturate. The
     * format must have a single component.
     *
     * @param format Layout written at the offset.
     */
    void write(
        const ElementRef& ref,
        const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
        const std::string& shader_path,
        uint32_t offset,
        Kakshya::GpuDataFormat format);

    /**
     * @brief Route element value to a descriptor binding on @p target_buffer.
     *
     * Creates a FormaBindingsProcessor if one does not yet exist for this element
     * and attaches it to @p target_buffer. If a processor already exists from a
     * prior write() call, @p target_buffer must be the same buffer — a second
     * attachment is not made.
     *
     * @param ref             Owning Layer and element id.
     * @param target_buffer   Buffer whose pipeline context receives the descriptor update.
     * @param shader_path     Shader path used to construct FormaBindingsProcessor if needed.
     * @param descriptor_name Descriptor name in the shader config.
     * @param binding_index   Vulkan binding index.
     * @param set             Descriptor set index.
     * @param role            UNIFORM or STORAGE.
     */
    void write(const ElementRef& ref,
        const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
        const std::string& shader_path,
        const std::string& descriptor_name,
        uint32_t binding_index,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM);

    /**
     * @brief Route element value to an AudioWriteProcessor each frame.
     * @param ref    Owning Layer and element id.
     * @param target AudioWriteProcessor to call set_data() on.
     */
    void write(const ElementRef& ref, std::shared_ptr<Buffers::AudioWriteProcessor> target);

    /**
     * @brief Route element value to a DataWriteProcessor each frame.
     * @param ref    Owning Layer and element id.
     * @param target DataWriteProcessor to call set_data() on.
     */
    void write(const ElementRef& ref, std::shared_ptr<Buffers::DataWriteProcessor> target);

    /**
     * @brief Route element value into a Constant node via set_constant().
     * @param ref  Owning Layer and element id.
     * @param node Constant node updated each frame.
     */
    void write(const ElementRef& ref, std::shared_ptr<Nodes::Constant> node);

    /**
     * @brief Route element bulk value to a caller-supplied sink each frame.
     *
     * When the element's MappedState<T> has a bulk_reader (T is vector<float>,
     * vector<double> or a glm vector), the full value is forwarded as a span.
     * For scalar T the sink receives a single-element span of the scalar value.
     *
     * Intended for consumers that operate on coefficient arrays or other
     * N-element state: FIR/IIR coefficient vectors, Random node bounds,
     * any function accepting a contiguous float range.
     *
     * @param ref  Owning Layer and element id.
     * @param sink Callable invoked each frame with the current value span.
     */
    void write(const ElementRef& ref, std::function<void(std::span<const float>)> sink);

    /**
     * @brief Route an ordered set of scalar states as one float range each frame.
     *
     * The range order is exactly the order of @p states. This is a
     * layer-owned route rather than an element-owned one: it does not consume
     * an element id, replace a control's existing outbound bindings, or
     * require a synthetic owner element. The task stops when @p layer stops.
     *
     * @param layer  Layer whose lifetime owns the aggregate route.
     * @param states Non-null scalar states to sample in range order.
     * @param sink   Consumer of the sampled range. The span is valid only
     *               for the duration of the call.
     */
    void write(
        Layer& layer,
        const std::vector<std::shared_ptr<MappedState<float>>>& states,
        std::function<void(std::span<const float>)> sink);

    /**
     * @brief Assemble a typed state range into DataVariant for a write processor.
     *
     * Each tick copies the current values of @p states, in order, into a
     * contiguous span of their native @p T type. @p assemble authors the
     * conversion from that span to DataVariant. Bridge owns the repeated
     * sampling, layer lifetime, and delivery through
     * @c Processor::set_data(DataVariant).
     *
     * @tparam T         Mapped state value type.
     * @tparam Processor Destination exposing set_data(DataVariant).
     * @tparam Assemble  Callable returning DataVariant from span<const T>.
     * @param layer    Layer whose lifetime owns the aggregate route.
     * @param states   Non-null states to sample in range order.
     * @param assemble Author-supplied conversion into DataVariant.
     * @param target   Processor receiving each assembled result.
     */
    template <typename T, typename Processor, typename Assemble>
    void write(
        Layer& layer,
        const std::vector<std::shared_ptr<MappedState<T>>>& states,
        Assemble assemble,
        std::shared_ptr<Processor> target)
    {
        static_assert(std::is_same_v<
                          std::remove_cvref_t<std::invoke_result_t<Assemble&, std::span<const T>>>,
                          Kakshya::DataVariant>,
            "Bridge aggregate assembly must return Kakshya::DataVariant");

        auto sources = states;
        auto assembler = std::make_shared<Assemble>(std::move(assemble));
        auto values = std::make_shared<std::vector<T>>();

        write_assembled_data(layer, [sources = std::move(sources), assembler, values]() mutable {
                values->resize(sources.size());
                for (size_t i = 0; i < sources.size(); ++i)
                    (*values)[i] = sources[i]->value;
                return (*assembler)(std::span<const T>(*values)); }, [target = std::move(target)](Kakshya::DataVariant data) { target->set_data(std::move(data)); });
    }

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
        write(ref_of(state), std::move(target_buffer), shader_path, offset, size);
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::VKBuffer> target_buffer,
        const std::string& shader_path,
        uint32_t offset,
        Kakshya::GpuDataFormat format)
    {
        write(ref_of(state), std::move(target_buffer), shader_path, offset, format);
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
        write(ref_of(state), std::move(target_buffer), shader_path,
            descriptor_name, binding_index, set, role);
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::AudioWriteProcessor> target)
    {
        write(ref_of(state), std::move(target));
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::DataWriteProcessor> target)
    {
        write(ref_of(state), std::move(target));
    }

    template <typename T>
    void write(std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Nodes::Constant> node)
    {
        write(ref_of(state), std::move(node));
    }

    // =========================================================================
    // Registration — called by Forma::create_element to make state findable
    // =========================================================================

    /**
     * @brief Register a MappedState<T> so that MappedState overloads and
     *        outbound bindings can resolve to the correct element and reader.
     *
     * Called by Forma::create_element. The reader closes over the MappedState
     * and is stored type-erased as std::function<float()>. The state records
     * @p ref so later calls made through it address the same element.
     * Rejected, with an error logged, when the Layer in @p ref has no
     * shared_ptr owner.
     *
     * @tparam T        MappedState value type.
     * @param ref       Owning Layer and the element id from Layer::add.
     * @param state     MappedState to register.
     * @param buffer    FormaBuffer the element renders into.
     * @param project   Optional T -> float projection. Identity cast if empty.
     */
    template <typename T>
    void register_element(
        const ElementRef& ref,
        std::shared_ptr<MappedState<T>> state,
        std::shared_ptr<Buffers::FormaBuffer> buffer,
        std::function<float(T)> project = {})
    {
        if (ref.first.expired()) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
                "Bridge::register_element: element {} belongs to a Layer with no shared owner", ref.second);
            return;
        }

        const uint32_t id = ref.second;

        std::function<float()> reader;
        if (project) {
            reader = [s = state, p = std::move(project)] {
                return p(s->value);
            };
        } else if constexpr (std::is_convertible_v<T, float>) {
            reader = [s = state] {
                return static_cast<float>(s->value);
            };
        } else {
            reader = [id, reported = std::make_shared<bool>(false)] {
                if (!*reported) {
                    *reported = true;
                    MF_RT_WARN(Journal::Component::Portal, Journal::Context::Init,
                        "Bridge: element {} has state type '{}' with no float conversion and no projection; its outbound reader yields 0",
                        id, typeid(T).name());
                }
                return 0.F;
            };
        }

        std::function<std::vector<float>()> bulk_reader;
        if constexpr (std::is_same_v<T, std::vector<float>>) {
            bulk_reader = [s = state] { return s->value; };
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            bulk_reader = [s = state] {
                std::vector<float> out;
                out.reserve(s->value.size());
                for (double v : s->value)
                    out.push_back(static_cast<float>(v));
                return out;
            };
        } else if constexpr (std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4>) {
            bulk_reader = [s = state] {
                return std::vector<float>(&s->value[0], &s->value[0] + T::length());
            };
        }

        std::function<void(float)> writer;
        if constexpr (std::is_convertible_v<float, T>) {
            writer = [s = state](float v) {
                s->write(static_cast<T>(v));
            };
        } else {
            writer = [id, reported = std::make_shared<bool>(false)](float) {
                if (!*reported) {
                    *reported = true;
                    MF_RT_WARN(Journal::Component::Portal, Journal::Context::Init,
                        "Bridge: element {} has state type '{}' with no float conversion and no projection; its inbound writer does nothing",
                        id, typeid(T).name());
                }
            };
        }

        state->id = id;
        state->layer = ref.first;
        m_records[ref.first][id] = ElementRecord {
            .buffer = std::move(buffer),
            .bindings = nullptr,
            .reader = std::move(reader),
            .bulk_reader = std::move(bulk_reader),
            .writer = std::move(writer),
            .inbound_task = {},
            .outbound_tasks = {},
            .binding_names = {},
        };
    }

    /**
     * @brief Register a Mapped element and scope its presentation sync to a Layer.
     *
     * Each frame's sync pushes the geometry function's bounds_hint and
     * contains back into @p layer, so hit testing follows the drawn shape.
     * The Layer scope also stops presentation sync when its Surface closes.
     * unbind(ref) cancels the sync task as well.
     *
     * @p layer must be owned by a shared_ptr, as Surface and
     * Forma::create_layer do; otherwise the call logs an error and does nothing.
     *
     * @param layer   Layer owning the element.
     * @param mapped  Fully constructed Mapped element.
     * @param project Optional T to float projection for outbound readers.
     */
    template <typename T>
    void register_element(
        Layer& layer,
        Mapped<T> mapped,
        std::function<float(T)> project = {})
    {
        auto owner = layer.weak_from_this();
        if (owner.expired()) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
                "Bridge::register_element: element {} belongs to a Layer with no shared owner", mapped.element.id);
            return;
        }

        register_element(ElementRef { std::move(owner), mapped.element.id },
            mapped.state, mapped.element.buffer, std::move(project));
        spawn_sync(layer, mapped.element.id,
            [m = std::move(mapped), target = &layer]() mutable { m.sync(target); });
    }

    // =========================================================================
    // Binding lifecycle
    // =========================================================================

    /**
     * @brief Cancel all inbound and outbound bindings for an element.
     *        The element remains in its layer.
     */
    void unbind(const ElementRef& ref);

    /**
     * @brief Stop Bridge tasks owned by a Layer.
     *
     * Stops presentation sync and aggregate routes. Element-owned
     * bindings associated with individual ids remain active.
     *
     * @param layer Layer whose Bridge tasks should stop.
     */
    void stop_sync(Layer& layer);

    template <typename T>
    void unbind(std::shared_ptr<MappedState<T>> state)
    {
        unbind(ref_of(state));
    }

    // =========================================================================
    // Binding handle
    // =========================================================================

    /**
     * @class Binding
     * @brief Non-owning handle for all bind/write/unbind operations on one element.
     *
     * Obtained via Bridge::at(ref) or Bridge::at(state). Closes over the
     * Bridge reference and the ElementRef so the caller never repeats either.
     * Every method forwards directly to the corresponding Bridge overload:
     * there is no deferred execution, no new mechanism.
     *
     * Binding holds a reference to its Bridge and must not outlive it.
     *
     * @code
     * bridge.at(el.state)
     *     .bind(envelope_node)
     *     .write(compute_buf, shader_path, offsetof(PC, cutoff))
     *     .write(audio_proc);
     * @endcode
     */
    class Binding {
    public:
        Binding(Bridge& bridge, ElementRef ref) noexcept
            : m_bridge(bridge)
            , m_ref(std::move(ref))
        {
        }

        [[nodiscard]] uint32_t id() const noexcept { return m_ref.second; }

        Binding& bind(std::shared_ptr<Nodes::Node> node,
            std::function<float(double)> project = {})
        {
            m_bridge.bind(m_ref, std::move(node), std::move(project));
            return *this;
        }

        Binding& bind(std::function<float()> source)
        {
            m_bridge.bind(m_ref, std::move(source));
            return *this;
        }

        Binding& write(
            const std::shared_ptr<Buffers::VKBuffer>& target,
            const std::string& shader_path,
            uint32_t offset,
            size_t size = sizeof(float))
        {
            m_bridge.write(m_ref, target, shader_path, offset, size);
            return *this;
        }

        Binding& write(
            const std::shared_ptr<Buffers::VKBuffer>& target,
            const std::string& shader_path,
            uint32_t offset,
            Kakshya::GpuDataFormat format)
        {
            m_bridge.write(m_ref, target, shader_path, offset, format);
            return *this;
        }

        Binding& write(
            const std::shared_ptr<Buffers::VKBuffer>& target,
            const std::string& shader_path,
            const std::string& descriptor_name,
            uint32_t binding_index,
            uint32_t set,
            Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM)
        {
            m_bridge.write(m_ref, target, shader_path,
                descriptor_name, binding_index, set, role);
            return *this;
        }

        Binding& write(std::shared_ptr<Buffers::AudioWriteProcessor> target)
        {
            m_bridge.write(m_ref, std::move(target));
            return *this;
        }

        Binding& write(std::shared_ptr<Buffers::DataWriteProcessor> target)
        {
            m_bridge.write(m_ref, std::move(target));
            return *this;
        }

        Binding& write(std::shared_ptr<Nodes::Constant> node)
        {
            m_bridge.write(m_ref, std::move(node));
            return *this;
        }

        Binding& write(std::function<void(std::span<const float>)> sink)
        {
            m_bridge.write(m_ref, std::move(sink));
            return *this;
        }

        void unbind()
        {
            m_bridge.unbind(m_ref);
        }

    private:
        Bridge& m_bridge;
        ElementRef m_ref;
    };

    // =========================================================================
    // at() — entry point for the Binding handle
    // =========================================================================

    /**
     * @brief Return a Binding handle for the element registered under @p ref.
     *
     * The handle is lightweight and non-owning. Prefer this over repeated
     * bind(ref, ...) / write(ref, ...) calls when wiring one element to
     * multiple sources or destinations.
     */
    [[nodiscard]] Binding at(ElementRef ref)
    {
        return Binding { *this, std::move(ref) };
    }

    /**
     * @brief Return a Binding handle resolved from a MappedState.
     */
    template <typename T>
    [[nodiscard]] Binding at(const std::shared_ptr<MappedState<T>>& state)
    {
        return Binding(*this, ref_of(state));
    }

    /**
     * @brief Spawn a presentation sync task scoped to a Layer.
     *
     * The task name is also stored in the outbound_tasks of the record for
     * @p id, when one exists, so that unbind() cancels it. @p layer must be
     * owned by a shared_ptr.
     *
     * @param layer Layer that owns the task.
     * @param id Element id used for task naming and diagnostics.
     * @param sync_fn Function invoked once per graphics frame.
     */
    void spawn_sync(Layer& layer, uint32_t id, std::function<void()> sync_fn);

private:
    struct ElementRecord {
        std::shared_ptr<Buffers::FormaBuffer> buffer;
        std::shared_ptr<Buffers::FormaBindingsProcessor> bindings;
        std::function<float()> reader; ///< reads current value from MappedState (outbound)
        std::function<std::vector<float>()> bulk_reader; ///< populated for vector<float>/vector<double> T
        std::function<void(float)> writer; ///< writes new value into MappedState (inbound)
        std::string inbound_task;
        std::vector<std::string> outbound_tasks;
        std::vector<std::string> binding_names;
    };

    Vruta::TaskScheduler& m_scheduler;
    Buffers::BufferManager& m_buffer_manager;

    mutable uint32_t m_next_id { 0 };

    std::map<std::weak_ptr<Layer>, std::unordered_map<uint32_t, ElementRecord>, std::owner_less<>> m_records;
    std::map<std::weak_ptr<Layer>, std::vector<std::string>, std::owner_less<>> m_layer_tasks;

    template <typename T>
    [[nodiscard]] static ElementRef ref_of(const std::shared_ptr<MappedState<T>>& state)
    {
        return { state->layer, state->id };
    }

    [[nodiscard]] ElementRecord* find_record(const ElementRef& ref);
    std::string make_task_name(uint32_t id, const char* suffix) const;
    std::string make_layer_task_name(const char* suffix) const;
    void cancel_inbound(ElementRecord& rec);
    void cancel_outbound(ElementRecord& rec);
    void spawn_inbound(const ElementRef& ref, std::function<float()> source);
    void write_aggregate(
        Layer& layer,
        std::vector<std::function<float()>> readers,
        std::function<void(std::span<const float>)> sink);
    void write_assembled_data(
        Layer& layer,
        std::function<Kakshya::DataVariant()> assemble,
        std::function<void(Kakshya::DataVariant)> write);
};

} // namespace MayaFlux::Portal::Forma
