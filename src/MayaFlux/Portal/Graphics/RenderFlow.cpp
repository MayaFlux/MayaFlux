#include "RenderFlow.hpp"

#include "LayoutTranslator.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool RenderFlow::s_initialized = false;

namespace {

    /**
     * @brief Translate semantic VertexLayout to Vulkan bindings/attributes
     * @param layout Semantic vertex layout
     * @return Tuple of (bindings, attributes)
     */
    std::pair<
        std::vector<Core::VertexBinding>,
        std::vector<Core::VertexAttribute>>
    translate_semantic_layout(const Kakshya::VertexLayout& layout)
    {
        using namespace MayaFlux::Portal::Graphics;

        auto [vk_bindings, vk_attributes] = VertexLayoutTranslator::translate_layout(layout);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Translated semantic vertex layout: {} bindings, {} attributes",
            vk_bindings.size(), vk_attributes.size());

        return { vk_bindings, vk_attributes };
    }

    template <typename T>
    constexpr size_t hash_bits(const T& value) noexcept
    {
        if constexpr (std::is_enum_v<T>) {
            return static_cast<size_t>(std::to_underlying(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            return std::bit_cast<std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>>(value);
        } else if constexpr (std::is_pointer_v<T>) {
            return std::bit_cast<size_t>(value);
        } else if constexpr (requires { typename T::MaskType; }) {
            return static_cast<size_t>(static_cast<typename T::MaskType>(value));
        } else {
            return static_cast<size_t>(value);
        }
    }

    template <typename... Ts>
    constexpr void mix_hash(size_t& seed, const Ts&... values) noexcept
    {
        constexpr size_t k_multiplier = 0x517cc1b727220a95ULL;
        ((seed = (std::rotl(seed, 5) ^ hash_bits(values)) * k_multiplier), ...);
    }

} // anonymous namespace

struct RenderFlow::PipelineKey {
    size_t hash {};
    Core::GraphicsPipelineConfig config;
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> set_bindings;
    std::optional<uint32_t> push_set;
    std::vector<std::pair<uint32_t, uint32_t>> specialization_constants;

    void finalize() noexcept;

    bool operator==(const PipelineKey&) const = default;
};

struct RenderFlow::PipelineKeyHash {
    size_t operator()(const PipelineKey& key) const noexcept { return key.hash; }
};

struct RenderFlow::SharedPipelines {
    std::unordered_map<PipelineKey, std::weak_ptr<PipelineState>, PipelineKeyHash> entries;
};

RenderFlow::RenderFlow()
    : m_shared_pipelines(std::make_unique<SharedPipelines>())
{
}

RenderFlow::~RenderFlow()
{
    shutdown();
}

void RenderFlow::PipelineKey::finalize() noexcept
{
    size_t seed = 0;

    mix_hash(seed, config.vertex_shader.get(), config.fragment_shader.get(),
        config.geometry_shader.get(), config.tess_control_shader.get(),
        config.tess_evaluation_shader.get(), config.mesh_shader.get(),
        config.task_shader.get());

    mix_hash(seed, config.topology, config.polygon_mode, config.cull_mode,
        config.front_face, config.line_width, config.depth_test_enable,
        config.depth_write_enable, config.depth_compare_op, config.stencil_test_enable,
        config.depth_clamp_enable, config.depth_bias_enable,
        config.depth_attachment_format);

    for (const auto& binding : config.vertex_bindings) {
        mix_hash(seed, binding.binding, binding.stride);
    }
    for (const auto& attribute : config.vertex_attributes) {
        mix_hash(seed, attribute.location, attribute.format, attribute.offset);
    }
    for (const auto& blend : config.color_blend_attachments) {
        mix_hash(seed, blend.blend_enable, blend.src_color_blend_factor,
            blend.dst_color_blend_factor);
    }
    for (const auto format : config.color_attachment_formats) {
        mix_hash(seed, format);
    }
    for (const auto& range : config.push_constant_ranges) {
        mix_hash(seed, range.stageFlags, range.size);
    }

    mix_hash(seed, push_set.value_or(UINT32_MAX));
    for (const auto& set : set_bindings) {
        mix_hash(seed, set.size());
        for (const auto& binding : set) {
            mix_hash(seed, binding.binding, binding.descriptorType, binding.descriptorCount);
        }
    }
    for (const auto& [constant_id, value] : specialization_constants) {
        mix_hash(seed, constant_id, value);
    }

    hash = seed;
}

//==============================================================================
// Initialization
//==============================================================================

bool RenderFlow::initialize()
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow already initialized (static flag)");
        return true;
    }

    if (m_shader_foundry) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow already initialized");
        return true;
    }

    auto* foundry = &ShaderFoundry::instance();
    if (!foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "ShaderFoundry must be initialized before RenderFlow");
        return false;
    }

    auto* display_service = Registry::BackendRegistry::instance()
                                .get_service<Registry::Service::DisplayService>();

    if (!display_service) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "DisplayService not found in BackendRegistry");
        return false;
    }

    m_shader_foundry = foundry;
    m_display_service = display_service;
    try {
        m_pipeline_cache = foundry->get_device().createPipelineCache({});
    } catch (const std::exception& e) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Vulkan pipeline cache unavailable, continuing without it: {}", e.what());
    }
    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "RenderFlow initialized");
    return true;
}

void RenderFlow::stop()
{
    if (!s_initialized) {
        return;
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "RenderFlow stopped");
}

void RenderFlow::shutdown()
{
    if (!s_initialized) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "Shutting down RenderFlow...");

    if (!m_shader_foundry || !m_shader_foundry->is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot shutdown RenderFlow: ShaderFoundry not initialized");
        return;
    }

    auto device = m_shader_foundry->get_device();

    if (!device) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot shutdown RenderFlow: Vulkan device is null");
        return;
    }

    cleanup_pipelines();

    if (m_pipeline_cache) {
        device.destroyPipelineCache(m_pipeline_cache);
        m_pipeline_cache = nullptr;
    }

    m_window_associations.clear();

    m_shader_foundry = nullptr;
    m_display_service = nullptr;

    s_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "RenderFlow shutdown complete");
}

//==============================================================================
// Utility Conversions (Portal enums → Vulkan enums)
//==============================================================================

namespace {

    vk::PrimitiveTopology to_vk_topology(PrimitiveTopology topology)
    {
        switch (topology) {
        case PrimitiveTopology::POINT_LIST:
            return vk::PrimitiveTopology::ePointList;
        case PrimitiveTopology::LINE_LIST:
            return vk::PrimitiveTopology::eLineList;
        case PrimitiveTopology::LINE_STRIP:
            return vk::PrimitiveTopology::eLineStrip;
        case PrimitiveTopology::TRIANGLE_LIST:
            return vk::PrimitiveTopology::eTriangleList;
        case PrimitiveTopology::TRIANGLE_STRIP:
            return vk::PrimitiveTopology::eTriangleStrip;
        case PrimitiveTopology::TRIANGLE_FAN:
            return vk::PrimitiveTopology::eTriangleFan;
        default:
            return vk::PrimitiveTopology::eTriangleList;
        }
    }

    vk::PolygonMode to_vk_polygon_mode(PolygonMode mode)
    {
        switch (mode) {
        case PolygonMode::FILL:
            return vk::PolygonMode::eFill;
        case PolygonMode::LINE:
            return vk::PolygonMode::eLine;
        case PolygonMode::POINT:
            return vk::PolygonMode::ePoint;
        default:
            return vk::PolygonMode::eFill;
        }
    }

    vk::CullModeFlags to_vk_cull_mode(CullMode mode)
    {
        switch (mode) {
        case CullMode::NONE:
            return vk::CullModeFlagBits::eNone;
        case CullMode::FRONT:
            return vk::CullModeFlagBits::eFront;
        case CullMode::BACK:
            return vk::CullModeFlagBits::eBack;
        case CullMode::FRONT_AND_BACK:
            return vk::CullModeFlagBits::eFrontAndBack;
        default:
            return vk::CullModeFlagBits::eBack;
        }
    }

    vk::CompareOp to_vk_compare_op(CompareOp op)
    {
        switch (op) {
        case CompareOp::NEVER:
            return vk::CompareOp::eNever;
        case CompareOp::LESS:
            return vk::CompareOp::eLess;
        case CompareOp::EQUAL:
            return vk::CompareOp::eEqual;
        case CompareOp::LESS_OR_EQUAL:
            return vk::CompareOp::eLessOrEqual;
        case CompareOp::GREATER:
            return vk::CompareOp::eGreater;
        case CompareOp::NOT_EQUAL:
            return vk::CompareOp::eNotEqual;
        case CompareOp::GREATER_OR_EQUAL:
            return vk::CompareOp::eGreaterOrEqual;
        case CompareOp::ALWAYS:
            return vk::CompareOp::eAlways;
        default:
            return vk::CompareOp::eLess;
        }
    }

    vk::BlendFactor to_vk_blend_factor(BlendFactor factor)
    {
        switch (factor) {
        case BlendFactor::ZERO:
            return vk::BlendFactor::eZero;
        case BlendFactor::ONE:
            return vk::BlendFactor::eOne;
        case BlendFactor::SRC_COLOR:
            return vk::BlendFactor::eSrcColor;
        case BlendFactor::ONE_MINUS_SRC_COLOR:
            return vk::BlendFactor::eOneMinusSrcColor;
        case BlendFactor::DST_COLOR:
            return vk::BlendFactor::eDstColor;
        case BlendFactor::ONE_MINUS_DST_COLOR:
            return vk::BlendFactor::eOneMinusDstColor;
        case BlendFactor::SRC_ALPHA:
            return vk::BlendFactor::eSrcAlpha;
        case BlendFactor::ONE_MINUS_SRC_ALPHA:
            return vk::BlendFactor::eOneMinusSrcAlpha;
        case BlendFactor::DST_ALPHA:
            return vk::BlendFactor::eDstAlpha;
        case BlendFactor::ONE_MINUS_DST_ALPHA:
            return vk::BlendFactor::eOneMinusDstAlpha;
        default:
            return vk::BlendFactor::eOne;
        }
    }

    vk::BlendOp to_vk_blend_op(BlendOp op)
    {
        switch (op) {
        case BlendOp::ADD:
            return vk::BlendOp::eAdd;
        case BlendOp::SUBTRACT:
            return vk::BlendOp::eSubtract;
        case BlendOp::REVERSE_SUBTRACT:
            return vk::BlendOp::eReverseSubtract;
        case BlendOp::MIN:
            return vk::BlendOp::eMin;
        case BlendOp::MAX:
            return vk::BlendOp::eMax;
        default:
            return vk::BlendOp::eAdd;
        }
    }
} // anonymous namespace

//==============================================================================
// Pipeline Creation
//==============================================================================

RenderPipelineID RenderFlow::create_pipeline(
    const RenderPipelineConfig& config,
    const std::vector<vk::Format>& color_formats,
    vk::Format depth_format,
    std::optional<uint32_t> push_set,
    const std::unordered_map<uint32_t, uint32_t>& specialization_constants)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "RenderFlow not initialized");
        return INVALID_RENDER_PIPELINE;
    }

    if (config.mesh_shader == INVALID_SHADER && config.vertex_shader == INVALID_SHADER) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline requires either mesh shader or vertex shader");
        return INVALID_RENDER_PIPELINE;
    }

    if (color_formats.empty()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "At least one color format required for dynamic rendering pipeline");
        return INVALID_RENDER_PIPELINE;
    }

    Core::GraphicsPipelineConfig vk_config;

    vk_config.vertex_shader = m_shader_foundry->get_vk_shader_module(config.vertex_shader);
    if (config.fragment_shader != INVALID_SHADER) {
        vk_config.fragment_shader = m_shader_foundry->get_vk_shader_module(config.fragment_shader);
    }
    if (config.geometry_shader != INVALID_SHADER) {
        vk_config.geometry_shader = m_shader_foundry->get_vk_shader_module(config.geometry_shader);
    }
    if (config.tess_control_shader != INVALID_SHADER) {
        vk_config.tess_control_shader = m_shader_foundry->get_vk_shader_module(config.tess_control_shader);
    }
    if (config.tess_eval_shader != INVALID_SHADER) {
        vk_config.tess_evaluation_shader = m_shader_foundry->get_vk_shader_module(config.tess_eval_shader);
    }

    vk_config.mesh_shader = m_shader_foundry->get_vk_shader_module(config.mesh_shader);
    if (config.task_shader != INVALID_SHADER) {
        vk_config.task_shader = m_shader_foundry->get_vk_shader_module(config.task_shader);
    }

    if (config.fragment_shader != INVALID_SHADER) {
        vk_config.fragment_shader = m_shader_foundry->get_vk_shader_module(config.fragment_shader);
    }

    if (config.semantic_vertex_layout.has_value()) {
        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline using semantic VertexLayout ({} vertices, {} attributes)",
            config.semantic_vertex_layout->vertex_count,
            config.semantic_vertex_layout->attributes.size());

        auto [vk_bindings, vk_attributes] = translate_semantic_layout(
            config.semantic_vertex_layout.value());

        vk_config.vertex_bindings = vk_bindings;
        vk_config.vertex_attributes = vk_attributes;
        vk_config.use_vertex_shader_reflection = false;

    } else if (!config.vertex_bindings.empty() || !config.vertex_attributes.empty()) {
        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline using explicit vertex config ({} bindings, {} attributes)",
            config.vertex_bindings.size(), config.vertex_attributes.size());

        for (const auto& binding : config.vertex_bindings) {
            Core::VertexBinding vk_binding {};
            vk_binding.binding = binding.binding;
            vk_binding.stride = binding.stride;
            vk_binding.input_rate = binding.per_instance ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
            vk_config.vertex_bindings.push_back(vk_binding);
        }

        for (const auto& attr : config.vertex_attributes) {
            Core::VertexAttribute vk_attr {};
            vk_attr.location = attr.location;
            vk_attr.binding = attr.binding;
            vk_attr.format = attr.format;
            vk_attr.offset = attr.offset;
            vk_config.vertex_attributes.push_back(vk_attr);
        }

        vk_config.use_vertex_shader_reflection = false;
    } else {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline will use shader reflection for vertex input");
        vk_config.use_vertex_shader_reflection = config.use_vertex_shader_reflection;
    }

    vk_config.topology = to_vk_topology(config.topology);
    vk_config.primitive_restart_enable = false;

    vk_config.polygon_mode = to_vk_polygon_mode(config.rasterization.polygon_mode);
    vk_config.cull_mode = to_vk_cull_mode(config.rasterization.cull_mode);
    vk_config.front_face = config.rasterization.front_face_ccw ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise;
    vk_config.line_width = config.rasterization.line_width;
    vk_config.depth_clamp_enable = config.rasterization.depth_clamp;
    vk_config.depth_bias_enable = config.rasterization.depth_bias;

    vk_config.depth_test_enable = config.depth_stencil.depth_test_enable;
    vk_config.depth_write_enable = config.depth_stencil.depth_write_enable;
    vk_config.depth_compare_op = to_vk_compare_op(config.depth_stencil.depth_compare_op);
    vk_config.stencil_test_enable = config.depth_stencil.stencil_test_enable;

    for (const auto& blend : config.blend_attachments) {
        Core::ColorBlendAttachment vk_blend;
        vk_blend.blend_enable = blend.blend_enable;
        vk_blend.src_color_blend_factor = to_vk_blend_factor(blend.src_color_factor);
        vk_blend.dst_color_blend_factor = to_vk_blend_factor(blend.dst_color_factor);
        vk_blend.color_blend_op = to_vk_blend_op(blend.color_blend_op);
        vk_blend.src_alpha_blend_factor = to_vk_blend_factor(blend.src_alpha_factor);
        vk_blend.dst_alpha_blend_factor = to_vk_blend_factor(blend.dst_alpha_factor);
        vk_blend.alpha_blend_op = to_vk_blend_op(blend.alpha_blend_op);
        vk_config.color_blend_attachments.push_back(vk_blend);
    }

    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> set_bindings;

    {
        std::vector<vk::DescriptorSetLayoutBinding> set0_bindings;

        vk::DescriptorSetLayoutBinding vt_b;
        vt_b.binding = 0;
        vt_b.descriptorType = vk::DescriptorType::eUniformBuffer;
        vt_b.descriptorCount = 1;
        vt_b.stageFlags = vk::ShaderStageFlagBits::eVertex;
        set0_bindings.push_back(vt_b);

        if (!config.descriptor_sets.empty()) {
            for (const auto& binding : config.descriptor_sets[0]) {
                if (binding.set == 0 && binding.binding != 0) {
                    vk::DescriptorSetLayoutBinding vk_b;
                    vk_b.binding = binding.binding;
                    vk_b.descriptorType = binding.type;
                    vk_b.descriptorCount = binding.count > 0 ? binding.count : 1;
                    vk_b.stageFlags = vk::ShaderStageFlagBits::eVertex
                        | vk::ShaderStageFlagBits::eFragment;
                    set0_bindings.push_back(vk_b);
                }
            }
        }

        set_bindings.push_back(std::move(set0_bindings));
    }

    for (const auto& desc_set : config.descriptor_sets) {
        bool all_set_zero = std::ranges::all_of(desc_set,
            [](const auto& b) { return b.set == 0; });
        if (all_set_zero)
            continue;

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const auto& binding : desc_set) {
            if (binding.set == 0)
                continue;
            vk::DescriptorSetLayoutBinding vk_b;
            vk_b.binding = binding.binding;
            vk_b.descriptorType = binding.type;
            vk_b.descriptorCount = binding.count > 0 ? binding.count : 1;
            vk_b.stageFlags = vk::ShaderStageFlagBits::eVertex
                | vk::ShaderStageFlagBits::eFragment;
            bindings.push_back(vk_b);
        }

        if (bindings.empty())
            continue;

        set_bindings.push_back(std::move(bindings));
    }

    if (push_set && *push_set >= set_bindings.size()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Push descriptor set {} is out of range for a pipeline with {} descriptor sets",
            *push_set, set_bindings.size());
        return INVALID_RENDER_PIPELINE;
    }

    vk::ShaderStageFlags push_constant_stages;

    if (config.mesh_shader != INVALID_SHADER) {
        push_constant_stages = vk::ShaderStageFlagBits::eMeshEXT;
        if (config.task_shader != INVALID_SHADER) {
            push_constant_stages |= vk::ShaderStageFlagBits::eTaskEXT;
        }
        if (config.fragment_shader != INVALID_SHADER) {
            push_constant_stages |= vk::ShaderStageFlagBits::eFragment;
        }
    } else {
        push_constant_stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        if (config.geometry_shader != INVALID_SHADER) {
            push_constant_stages |= vk::ShaderStageFlagBits::eGeometry;
        }
        if (config.tess_control_shader != INVALID_SHADER) {
            push_constant_stages |= vk::ShaderStageFlagBits::eTessellationControl;
        }
        if (config.tess_eval_shader != INVALID_SHADER) {
            push_constant_stages |= vk::ShaderStageFlagBits::eTessellationEvaluation;
        }
    }

    if (config.push_constant_size > 0) {
        vk::PushConstantRange range;
        range.stageFlags = push_constant_stages;
        range.offset = 0;
        range.size = static_cast<uint32_t>(config.push_constant_size);
        vk_config.push_constant_ranges.push_back(range);
    }

    vk_config.color_attachment_formats = color_formats;
    vk_config.depth_attachment_format = depth_format;

    vk_config.dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk_config.cache = m_pipeline_cache;

    std::vector<std::pair<uint32_t, uint32_t>> sorted_specialization(
        specialization_constants.begin(), specialization_constants.end());
    std::ranges::sort(sorted_specialization);

    PipelineKey pipeline_key {
        .config = std::move(vk_config),
        .set_bindings = std::move(set_bindings),
        .push_set = push_set,
        .specialization_constants = std::move(sorted_specialization)
    };
    pipeline_key.finalize();

    auto& shared_entries = m_shared_pipelines->entries;
    if (auto cached = shared_entries.find(pipeline_key); cached != shared_entries.end()) {
        if (auto shared = cached->second.lock()) {
            const auto shared_id = m_next_pipeline_id.fetch_add(1);
            m_pipelines[shared_id] = std::move(shared);
            MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
                "Graphics pipeline cache HIT (ID: {}, {} IDs share this state, {} unique states)",
                shared_id, m_pipelines[shared_id].use_count(), shared_entries.size());
            return shared_id;
        }
        shared_entries.erase(cached);
    }

    auto device = m_shader_foundry->get_device();

    std::vector<vk::DescriptorSetLayout> layouts;
    for (size_t index = 0; index < pipeline_key.set_bindings.size(); ++index) {
        const auto& bindings = pipeline_key.set_bindings[index];
        vk::DescriptorSetLayoutCreateInfo layout_info;
        if (push_set && *push_set == index) {
            layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor;
        }
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();
        layouts.push_back(device.createDescriptorSetLayout(layout_info));
    }

    const vk::DescriptorSetLayout state_vt_layout = layouts.front();
    auto create_config = pipeline_key.config;
    create_config.descriptor_set_layouts = layouts;

    auto pipeline = std::make_shared<Core::VKGraphicsPipeline>();
    if (!pipeline->create(device, create_config, specialization_constants)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Failed to create VKGraphicsPipeline for dynamic rendering");

        for (auto layout : layouts) {
            device.destroyDescriptorSetLayout(layout);
        }
        return INVALID_RENDER_PIPELINE;
    }

    auto pipeline_id = m_next_pipeline_id.fetch_add(1);
    auto state = std::make_shared<PipelineState>();
    state->device = device;
    state->shader_ids = { config.vertex_shader, config.fragment_shader };
    state->pipeline = pipeline;
    state->layouts = layouts;
    state->view_transform_layout = state_vt_layout;
    state->push_set = push_set;
    if (push_set) {
        for (const ShaderID shader_id : { config.vertex_shader, config.fragment_shader, config.geometry_shader,
                 config.tess_control_shader, config.tess_eval_shader, config.mesh_shader, config.task_shader }) {
            if (shader_id == INVALID_SHADER) {
                continue;
            }
            for (const auto& used : m_shader_foundry->get_active_descriptor_bindings(shader_id)) {
                if (used.set == *push_set
                    && std::ranges::find(state->push_required, used.binding) == state->push_required.end()) {
                    state->push_required.push_back(used.binding);
                }
            }
        }
    }
    state->layout = pipeline->get_layout();
    state->push_constant_stages = push_constant_stages;
    shared_entries.insert_or_assign(std::move(pipeline_key), state);
    m_pipelines[pipeline_id] = std::move(state);

    MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
        "Graphics pipeline cache MISS, created (ID: {}, {} color attachments, {} unique states)",
        pipeline_id, color_formats.size(), shared_entries.size());

    return pipeline_id;
}

void RenderFlow::destroy_pipeline(RenderPipelineID pipeline_id)
{
    auto it = m_pipelines.find(pipeline_id);
    if (it == m_pipelines.end()) {
        return;
    }

    m_pipelines.erase(it);
    std::erase_if(m_shared_pipelines->entries,
        [](const auto& entry) { return entry.second.expired(); });

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "Destroyed graphics pipeline (ID: {})", pipeline_id);
}

RenderFlow::PipelineState::~PipelineState()
{
    if (!device) {
        return;
    }

    if (pipeline) {
        pipeline->cleanup(device);
    }

    for (auto layout : layouts) {
        if (layout) {
            device.destroyDescriptorSetLayout(layout);
        }
    }
}

void RenderFlow::cleanup_pipelines()
{
    m_shared_pipelines->entries.clear();
    m_pipelines.clear();

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "Cleaned up all graphics pipelines");
}

vk::DescriptorSetLayout RenderFlow::get_view_transform_layout(
    RenderPipelineID pipeline_id) const
{
    auto it = m_pipelines.find(pipeline_id);
    if (it == m_pipelines.end()) {
        return {};
    }
    return it->second->view_transform_layout;
}

//==============================================================================
// Dynamic Rendering
//==============================================================================

void RenderFlow::begin_rendering(
    CommandBufferID cmd_id,
    const std::shared_ptr<Core::Window>& window,
    vk::Image swapchain_image,
    const std::array<float, 4>& clear_color,
    vk::ImageView depth_image_view)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    if (!window) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot begin rendering for null window");
        return;
    }

    if (!swapchain_image) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot begin rendering with null swapchain image");
        return;
    }

    auto it = m_window_associations.find(window);
    if (it == m_window_associations.end()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Window '{}' not registered for rendering. "
            "Call register_window_for_rendering() first.",
            window->get_create_info().title);
        m_window_associations.emplace(window, WindowRenderAssociation { .window = window, .swapchain_image = swapchain_image });
    } else {
        it->second.swapchain_image = swapchain_image;
    }

    uint32_t width = 0, height = 0;
    m_display_service->get_swapchain_extent(window, width, height);

    if (width == 0 || height == 0) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid swapchain extent for window '{}': {}x{}",
            window->get_create_info().title, width, height);
        return;
    }

    vk::ImageView image_view = get_current_image_view(window);
    if (!image_view) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Failed to get image view for window '{}'",
            window->get_create_info().title);
        return;
    }

    vk::ImageMemoryBarrier2 pre_barrier {};
    pre_barrier.oldLayout = vk::ImageLayout::eUndefined;
    pre_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    pre_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre_barrier.image = swapchain_image;
    pre_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    pre_barrier.subresourceRange.baseMipLevel = 0;
    pre_barrier.subresourceRange.levelCount = 1;
    pre_barrier.subresourceRange.baseArrayLayer = 0;
    pre_barrier.subresourceRange.layerCount = 1;
    pre_barrier.srcStageMask = vk::PipelineStageFlagBits2::eNone;
    pre_barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
    pre_barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    pre_barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;

    vk::DependencyInfo pre_dependency;
    pre_dependency.imageMemoryBarrierCount = 1;
    pre_dependency.pImageMemoryBarriers = &pre_barrier;
    cmd.pipelineBarrier2(pre_dependency);

    vk::RenderingAttachmentInfo color_attachment {};
    color_attachment.sType = vk::StructureType::eRenderingAttachmentInfo;
    color_attachment.pNext = nullptr;
    color_attachment.imageView = image_view;
    color_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.resolveMode = vk::ResolveModeFlagBits::eNone;
    color_attachment.resolveImageView = nullptr;
    color_attachment.resolveImageLayout = vk::ImageLayout::eUndefined;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;

    if (clear_color != default_color) {
        color_attachment.clearValue.color = vk::ClearColorValue(clear_color);
    } else {
        color_attachment.clearValue.color = vk::ClearColorValue(window->get_create_info().clear_color);
    }

    vk::RenderingAttachmentInfo depth_attachment {};
    if (depth_image_view) {
        depth_attachment.sType = vk::StructureType::eRenderingAttachmentInfo;
        depth_attachment.pNext = nullptr;
        depth_attachment.imageView = depth_image_view;
        depth_attachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depth_attachment.resolveMode = vk::ResolveModeFlagBits::eNone;
        depth_attachment.resolveImageView = nullptr;
        depth_attachment.resolveImageLayout = vk::ImageLayout::eUndefined;
        depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
        depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depth_attachment.clearValue.depthStencil = vk::ClearDepthStencilValue { 1.0F, 0 };
    }

    vk::RenderingInfo rendering_info {};
    rendering_info.sType = vk::StructureType::eRenderingInfo;
    rendering_info.pNext = nullptr;
    rendering_info.flags = vk::RenderingFlagBits::eContentsSecondaryCommandBuffers;
    rendering_info.renderArea.offset = vk::Offset2D { 0, 0 };
    rendering_info.renderArea.extent = vk::Extent2D { width, height };
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = depth_image_view ? &depth_attachment : nullptr;
    rendering_info.pStencilAttachment = nullptr;

    cmd.beginRendering(rendering_info);

    MF_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "Began dynamic rendering for window '{}' ({}x{}, depth: {})",
        window->get_create_info().title, width, height,
        depth_image_view ? "yes" : "no");
}

void RenderFlow::release_draw_inputs(CommandBufferID cmd_id)
{
    m_shader_foundry->memory_barrier(
        cmd_id,
        vk::AccessFlagBits2::eVertexAttributeRead | vk::AccessFlagBits2::eIndexRead
            | vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eUniformRead
            | vk::AccessFlagBits2::eShaderRead,
        vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eVertexInput | vk::PipelineStageFlagBits2::eVertexShader
            | vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eDrawIndirect,
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eTransfer);
}

void RenderFlow::end_rendering(
    CommandBufferID cmd_id,
    const std::shared_ptr<Core::Window>& window)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.endRendering();

    release_draw_inputs(cmd_id);

    auto it = m_window_associations.find(window);
    if (it == m_window_associations.end() || !it->second.swapchain_image) {
        MF_RT_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "No swapchain image tracked for window '{}'",
            window->get_create_info().title);
        return;
    }

    vk::ImageMemoryBarrier2 post_barrier {};
    post_barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    post_barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    post_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post_barrier.image = it->second.swapchain_image;
    post_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    post_barrier.subresourceRange.baseMipLevel = 0;
    post_barrier.subresourceRange.levelCount = 1;
    post_barrier.subresourceRange.baseArrayLayer = 0;
    post_barrier.subresourceRange.layerCount = 1;
    post_barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    post_barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    post_barrier.dstStageMask = vk::PipelineStageFlagBits2::eNone;
    post_barrier.dstAccessMask = vk::AccessFlagBits2::eNone;

    vk::DependencyInfo post_dependency;
    post_dependency.imageMemoryBarrierCount = 1;
    post_dependency.pImageMemoryBarriers = &post_barrier;
    cmd.pipelineBarrier2(post_dependency);

    it->second.swapchain_image = nullptr;

    MF_TRACE(Journal::Component::Portal, Journal::Context::Rendering,
        "Ended dynamic rendering for window '{}'",
        window->get_create_info().title);
}

//==============================================================================
// Command Recording
//==============================================================================

void RenderFlow::bind_pipeline(CommandBufferID cmd_id, RenderPipelineID pipeline_id)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second->pipeline->bind(cmd);
}

void RenderFlow::bind_vertex_buffers(
    CommandBufferID cmd_id,
    const std::vector<std::shared_ptr<Buffers::VKBuffer>>& buffers,
    uint32_t first_binding)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::Buffer> vk_buffers;
    std::vector<vk::DeviceSize> offsets(buffers.size(), 0);

    vk_buffers.reserve(buffers.size());
    for (const auto& buf : buffers) {
        vk_buffers.push_back(buf->get_buffer());
    }

    cmd.bindVertexBuffers(first_binding, vk_buffers, offsets);
}

void RenderFlow::bind_index_buffer(
    CommandBufferID cmd_id,
    const std::shared_ptr<Buffers::VKBuffer>& buffer,
    vk::IndexType index_type)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.bindIndexBuffer(buffer->get_index_buffer(), 0, index_type);
}

std::optional<uint32_t> RenderFlow::first_unwritten_push_binding(
    RenderPipelineID pipeline_id,
    const std::vector<vk::WriteDescriptorSet>& writes) const
{
    const auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end() || !pipeline_it->second->push_set) {
        return std::nullopt;
    }

    for (const uint32_t required : pipeline_it->second->push_required) {
        const bool written = std::ranges::any_of(writes, [required](const vk::WriteDescriptorSet& write) {
            return write.dstBinding == required;
        });
        if (!written) {
            return required;
        }
    }

    return std::nullopt;
}

void RenderFlow::push_descriptors(
    CommandBufferID cmd_id,
    RenderPipelineID pipeline_id,
    const std::vector<vk::WriteDescriptorSet>& writes)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    if (!pipeline_it->second->push_set) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Pipeline {} has no push descriptor set", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    pipeline_it->second->pipeline->push_descriptors(cmd, *pipeline_it->second->push_set, writes);
}

void RenderFlow::bind_descriptor_sets(
    CommandBufferID cmd_id,
    RenderPipelineID pipeline_id,
    const std::vector<DescriptorSetID>& descriptor_sets,
    uint32_t first_set)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    std::vector<vk::DescriptorSet> vk_sets;
    vk_sets.reserve(descriptor_sets.size());
    for (auto ds_id : descriptor_sets) {
        vk_sets.push_back(m_shader_foundry->get_descriptor_set(ds_id));
    }

    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline_it->second->layout,
        first_set,
        vk_sets,
        nullptr);
}

void RenderFlow::push_constants(
    CommandBufferID cmd_id,
    RenderPipelineID pipeline_id,
    const void* data,
    size_t size,
    uint32_t offset)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return;
    }

    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.pushConstants(
        pipeline_it->second->layout,
        pipeline_it->second->push_constant_stages,
        offset,
        static_cast<uint32_t>(size),
        data);
}

void RenderFlow::draw(
    CommandBufferID cmd_id,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void RenderFlow::draw_indexed(
    CommandBufferID cmd_id,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        return;
    }

    cmd.drawIndexed(index_count, instance_count, first_index,
        vertex_offset, first_instance);
}

void RenderFlow::draw_mesh_tasks(
    CommandBufferID cmd_id,
    uint32_t group_count_x,
    uint32_t group_count_y,
    uint32_t group_count_z)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid command buffer ID: {}", cmd_id);
        return;
    }

    cmd.drawMeshTasksEXT(group_count_x, group_count_y, group_count_z);
}

void RenderFlow::draw_mesh_tasks_indirect(
    CommandBufferID cmd_id,
    const std::shared_ptr<Buffers::VKBuffer>& buffer,
    vk::DeviceSize offset,
    uint32_t draw_count,
    uint32_t stride)
{
    auto cmd = m_shader_foundry->get_command_buffer(cmd_id);
    if (!cmd || !buffer)
        return;

    cmd.drawMeshTasksIndirectEXT(buffer->get_buffer(), offset, draw_count, stride);
}

//==========================================================================
// Window Rendering Registration
//==========================================================================

void RenderFlow::register_window_for_rendering(const std::shared_ptr<Core::Window>& window)
{
    if (!window) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Cannot register null window");
        return;
    }

    if (!window->is_graphics_registered()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Rendering,
            "Window '{}' not registered with graphics backend yet.",
            window->get_create_info().title);
    }

    if (!m_window_associations.contains(window)) {
        WindowRenderAssociation association;
        association.window = window;
        m_window_associations[window] = std::move(association);

        MF_INFO(Journal::Component::Portal, Journal::Context::Rendering,
            "Registered window '{}' for dynamic rendering",
            window->get_create_info().title);
    }
}

void RenderFlow::unregister_window(const std::shared_ptr<Core::Window>& window)
{
    if (!window) {
        return;
    }

    auto it = m_window_associations.find(window);

    if (it != m_window_associations.end()) {
        m_window_associations.erase(it);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
            "Unregistered window '{}' from rendering",
            window->get_create_info().title);
    }
}

bool RenderFlow::is_window_registered(const std::shared_ptr<Core::Window>& window) const
{
    return window && m_window_associations.contains(window);
}

std::vector<std::shared_ptr<Core::Window>> RenderFlow::get_registered_windows() const
{
    std::vector<std::shared_ptr<Core::Window>> windows;
    windows.reserve(m_window_associations.size());

    for (const auto& [key, association] : m_window_associations) {
        if (auto window = association.window.lock()) {
            windows.push_back(window);
        }
    }

    return windows;
}

vk::ImageView RenderFlow::get_current_image_view(const std::shared_ptr<Core::Window>& window)
{
    auto view_ptr = m_display_service->get_current_image_view(window);
    if (!view_ptr) {
        return nullptr;
    }
    return *static_cast<vk::ImageView*>(view_ptr);
}

//==============================================================================
// Convenience Methods
//==============================================================================

std::vector<DescriptorSetID> RenderFlow::allocate_pipeline_descriptors(
    RenderPipelineID pipeline_id,
    uint32_t first_layout_index)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
            "Invalid pipeline ID: {}", pipeline_id);
        return {};
    }

    const auto& layouts = pipeline_it->second->layouts;
    std::vector<DescriptorSetID> descriptor_set_ids;

    for (uint32_t i = first_layout_index; i < layouts.size(); ++i) {
        if (pipeline_it->second->push_set && *pipeline_it->second->push_set == i) {
            continue;
        }
        auto ds_id = m_shader_foundry->allocate_descriptor_set(layouts[i]);
        if (ds_id == INVALID_DESCRIPTOR_SET) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::Rendering,
                "Failed to allocate descriptor set for pipeline {}", pipeline_id);
            return {};
        }
        descriptor_set_ids.push_back(ds_id);
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::Rendering,
        "Allocated {} descriptor sets for pipeline {} (starting at layout index {})",
        descriptor_set_ids.size(), pipeline_id, first_layout_index);

    return descriptor_set_ids;
}

} // namespace MayaFlux::Portal::Graphics
