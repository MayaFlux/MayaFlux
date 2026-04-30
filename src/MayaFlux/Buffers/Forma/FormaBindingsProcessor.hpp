#pragma once

#include "MayaFlux/Buffers/Shaders/ShaderProcessor.hpp"

#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FormaBindingsProcessor
 * @brief ShaderProcessor that reads Forma element state and routes it to push
 *        constants or descriptor bindings on external pipelines each graphics tick.
 *
 * Attaches to a FormaBuffer as a secondary processor — never as the primary.
 * Owns no pipeline or shader of its own; initialize_pipeline() and
 * initialize_descriptors() are no-ops, matching NodeBindingsProcessor and
 * DescriptorBindingsProcessor.
 *
 * The processor is the automation layer. Each binding registers:
 *   - A type-erased reader: std::function<float()> that closes over a
 *     shared_ptr<MappedState<T>> and a projection function T -> float.
 *     The reader is called every graphics tick regardless of version — the
 *     graphics tick IS the sampling boundary.
 *   - A target: either a push constant slot on an external ShaderProcessor,
 *     or a descriptor buffer registered on the attached buffer's
 *     pipeline_context.
 *
 * Templated bind overloads accept any MappedState<T> and a projection
 * function, constructing the type-erased reader internally. The processor
 * itself holds no template parameters.
 *
 * Usage:
 * @code
 * // Fader state drives a push constant on a compute processor
 * forma_bindings->bind_push_constant(
 *     "cutoff",
 *     fader.state,
 *     [](float v) { return v; },
 *     compute_proc,
 *     offsetof(MyPC, cutoff));
 *
 * // 2D picker x-axis drives a descriptor uniform
 * forma_bindings->bind_descriptor(
 *     "pan",
 *     picker.state,
 *     [](glm::vec2 v) { return v.x; },
 *     "pan_ubo",
 *     0,
 *     Portal::Graphics::DescriptorRole::UNIFORM);
 *
 * forma_buffer->add_processor(forma_bindings);
 * @endcode
 */
class MAYAFLUX_API FormaBindingsProcessor : public ShaderProcessor {
public:
    explicit FormaBindingsProcessor(const std::string& shader_path);
    explicit FormaBindingsProcessor(ShaderConfig config);
    ~FormaBindingsProcessor() override = default;

    FormaBindingsProcessor(const FormaBindingsProcessor&) = delete;
    FormaBindingsProcessor& operator=(const FormaBindingsProcessor&) = delete;
    FormaBindingsProcessor(FormaBindingsProcessor&&) = delete;
    FormaBindingsProcessor& operator=(FormaBindingsProcessor&&) = delete;

    // =========================================================================
    // Push constant bindings — raw reader overload
    // =========================================================================

    /**
     * @brief Bind a type-erased reader to a push constant slot.
     *
     * Used by Bridge, which holds readers as std::function<float()> after
     * type-erasing MappedState<T> at register_element time.
     *
     * @param name     Logical binding name.
     * @param reader   Callable returning the current float value each tick.
     * @param offset   Byte offset in the push constant struct.
     * @param size     Byte width. Defaults to sizeof(float).
     */
    void bind_push_constant(
        const std::string& name,
        std::function<float()> reader,
        uint32_t offset,
        size_t size = sizeof(float));

    // =========================================================================
    // Descriptor bindings — raw reader overload
    // =========================================================================

    /**
     * @brief Bind a type-erased reader to a descriptor binding.
     *
     * Used by Bridge for the same reason as the push constant raw overload.
     *
     * @param name            Logical binding name.
     * @param reader          Callable returning the current float value each tick.
     * @param descriptor_name Descriptor name in the shader config.
     * @param binding_index   Vulkan binding index within the descriptor set.
     * @param set             Descriptor set index.
     * @param role            UNIFORM for UBO, STORAGE for SSBO.
     */
    void bind_descriptor(
        const std::string& name,
        std::function<float()> reader,
        const std::string& descriptor_name,
        uint32_t binding_index,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM);

    // =========================================================================
    // Push constant bindings — MappedState<T> overload

    /**
     * @brief Bind a MappedState<T> to a push constant slot on an external
     *        ShaderProcessor.
     *
     * @param name      Logical binding name for introspection and unbind.
     * @param state     MappedState whose value is read each graphics tick.
     * @param project   Projection from T to float. Called every tick.
     *                  the value at @p offset. Any subclass is valid.
     * @param offset    Byte offset in the target's push constant struct.
     * @param size      Byte width of the written value. Defaults to sizeof(float).
     * @tparam T        MappedState value type.
     */
    template <typename T>
    void bind_push_constant(
        const std::string& name,
        std::shared_ptr<Portal::Forma::MappedState<T>> state,
        std::function<float(T)> project,
        uint32_t offset,
        size_t size = sizeof(float))
    {
        if (!state) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "FormaBindingsProcessor::bind_push_constant: null state for '{}'", name);
            return;
        }

        auto& b = m_bindings[name];
        b.kind = TargetKind::PUSH_CONSTANT;
        b.reader = [s = std::move(state), p = std::move(project)]() {
            return p(s->value);
        };
        b.pc = PushConstantTarget { .offset = offset, .size = size };
        b.desc.reset();
    }

    // =========================================================================
    // Descriptor bindings
    // =========================================================================

    /**
     * @brief Bind a MappedState<T> to a descriptor binding on the attached buffer.
     *
     * Creates an owned host-visible GPU buffer sized to sizeof(float).
     * Each tick the projected value is uploaded into this buffer and
     * registered in the attached buffer's descriptor_buffer_bindings,
     * following the same path as DescriptorBindingsProcessor.
     *
     * @param name            Logical binding name.
     * @param state           MappedState whose value is read each graphics tick.
     * @param project         Projection from T to float. Called every tick.
     * @param descriptor_name Descriptor name in the shader config.
     * @param binding_index   Vulkan binding index within the descriptor set.
     * @param set             Descriptor set index.
     * @param role            UNIFORM for UBO, STORAGE for SSBO.
     * @tparam T              MappedState value type.
     */
    template <typename T>
    void bind_descriptor(
        const std::string& name,
        std::shared_ptr<Portal::Forma::MappedState<T>> state,
        std::function<float(T)> project,
        const std::string& descriptor_name,
        uint32_t binding_index,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM)
    {
        if (!state) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "FormaBindingsProcessor::bind_descriptor: null state for '{}'", name);
            return;
        }

        auto gpu_buf = make_descriptor_buffer(role);

        auto& b = m_bindings[name];
        b.kind = TargetKind::DESCRIPTOR;
        b.reader = [s = std::move(state), p = std::move(project)]() {
            return p(s->value);
        };
        b.pc.reset();
        b.desc = DescriptorTarget {
            .descriptor_name = descriptor_name,
            .set_index = set,
            .binding_index = binding_index,
            .role = role,
            .gpu_buffer = gpu_buf,
            .buffer_size = sizeof(float),
        };

        bind_buffer(descriptor_name, gpu_buf);
        m_needs_descriptor_rebuild = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBindingsProcessor::bind_descriptor: '{}' -> descriptor '{}' set={} binding={}",
            name, descriptor_name, set, binding_index);
    }

    // =========================================================================
    // Introspection
    // =========================================================================

    /** @brief Returns true if a binding with @p name exists. */
    [[nodiscard]] bool has_binding(const std::string& name) const;

    /** @brief Returns all registered binding names. */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

    /**
     * @brief Remove a binding by name.
     * @return True if found and removed.
     */
    bool unbind(const std::string& name);

protected:
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;
    void initialize_pipeline(const std::shared_ptr<VKBuffer>&) override { }
    void initialize_descriptors(const std::shared_ptr<VKBuffer>&) override { }

private:
    enum class TargetKind : uint8_t {
        PUSH_CONSTANT,
        DESCRIPTOR,
    };

    struct PushConstantTarget {
        uint32_t offset;
        size_t size;
    };

    struct DescriptorTarget {
        std::string descriptor_name;
        uint32_t set_index;
        uint32_t binding_index;
        Portal::Graphics::DescriptorRole role;
        std::shared_ptr<VKBuffer> gpu_buffer;
        size_t buffer_size;
    };

    struct Binding {
        TargetKind kind;
        std::function<float()> reader;
        std::optional<PushConstantTarget> pc;
        std::optional<DescriptorTarget> desc;
    };

    std::unordered_map<std::string, Binding> m_bindings;

    void flush_push_constant(float value, const PushConstantTarget& pc, const std::shared_ptr<VKBuffer>& buffer);

    void flush_descriptor(float value, const DescriptorTarget& desc,
        const std::shared_ptr<VKBuffer>& attached);

    [[nodiscard]] static std::shared_ptr<VKBuffer> make_descriptor_buffer(
        Portal::Graphics::DescriptorRole role);
};

} // namespace MayaFlux::Buffers
