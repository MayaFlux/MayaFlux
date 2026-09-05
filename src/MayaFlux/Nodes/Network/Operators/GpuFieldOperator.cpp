#include "GpuFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

namespace {

    /**
     * @brief GLSL type spelling for a component count.
     */
    [[nodiscard]] const char* glsl_type(uint32_t components) noexcept
    {
        switch (components) {
        case 1:
            return "float";
        case 2:
            return "vec2";
        default:
            return "vec3";
        }
    }

    /**
     * @brief Zero literal for a component count.
     */
    [[nodiscard]] std::string glsl_zero(uint32_t components)
    {
        return components == 1 ? "0.0f" : std::string(glsl_type(components)) + "(0.0f)";
    }

    [[nodiscard]] std::string word(uint32_t n)
    {
        return std::to_string(n) + "u";
    }

    [[nodiscard]] const char* target_name(Kinesis::FieldTarget t) noexcept
    {
        using Kinesis::FieldTarget;
        switch (t) {
        case FieldTarget::POSITION:
            return "position";
        case FieldTarget::COLOR:
            return "color";
        case FieldTarget::NORMAL:
            return "normal";
        case FieldTarget::TANGENT:
            return "tangent";
        case FieldTarget::SCALAR:
            return "scalar";
        case FieldTarget::UV:
            return "uv";
        default:
            return "unknown";
        }
    }

    /**
     * @brief Whether a target accumulates onto its current value or replaces it.
     *
     * Matches FieldOperator: a position field is a displacement added to the
     * vertex, every other target is written outright. FieldMode does not enter
     * into it, because mode there selects whether reference data is restored
     * before evaluation, and this operator holds no reference data.
     */
    [[nodiscard]] bool target_accumulates(Kinesis::FieldTarget t) noexcept
    {
        return t == Kinesis::FieldTarget::POSITION;
    }

    /**
     * @brief Vertex attribute modality a single-bit target addresses.
     */
    [[nodiscard]] std::optional<Kakshya::DataModality>
    target_modality(Kinesis::FieldTarget t) noexcept
    {
        using Kakshya::DataModality;
        using Kinesis::FieldTarget;
        switch (t) {
        case FieldTarget::POSITION:
            return DataModality::VERTEX_POSITIONS_3D;
        case FieldTarget::COLOR:
            return DataModality::VERTEX_COLORS_RGB;
        case FieldTarget::NORMAL:
            return DataModality::VERTEX_NORMALS_3D;
        case FieldTarget::TANGENT:
            return DataModality::VERTEX_TANGENTS_3D;
        case FieldTarget::SCALAR:
            return DataModality::SCALAR_F32;
        case FieldTarget::UV:
            return DataModality::TEXTURE_COORDS_2D;
        default:
            return std::nullopt;
        }
    }

    /**
     * @brief Component count implied by an attribute modality.
     */
    [[nodiscard]] uint32_t modality_components(Kakshya::DataModality m) noexcept
    {
        using Kakshya::DataModality;
        switch (m) {
        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_COLORS_RGB:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
            return 3;
        case DataModality::TEXTURE_COORDS_2D:
            return 2;
        case DataModality::SCALAR_F32:
            return 1;
        default:
            return 0;
        }
    }

    constexpr std::array<const char*, 3> k_swizzle { ".x", ".y", ".z" };

} // namespace

GpuFieldOperator::GpuFieldOperator(Kakshya::VertexLayout layout, SpatialFieldConfig config)
    : m_layout(std::move(layout))
    , m_field_config(config)
{
    if (m_layout.stride_bytes == 0 || m_layout.stride_bytes % 4 != 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator: stride {} is not a nonzero multiple of 4. The "
            "emitted shader addresses the record as a flat float array and "
            "cannot describe this layout. The operator is inert: bind() will "
            "reject everything and build_spec() returns nullopt.",
            m_layout.stride_bytes);
        return;
    }

    m_stride_words = m_layout.stride_bytes / 4;
}

void GpuFieldOperator::set_density_saturation_count(float count)
{
    m_field_config.density_saturation_count = count;
    invalidate();
}

void GpuFieldOperator::set_capture_growth(float growth)
{
    m_field_config.capture_growth = growth;
    invalidate();
}

void GpuFieldOperator::set_swallow_base_size(float size)
{
    m_field_config.swallow_base_size = size;
    invalidate();
}

void GpuFieldOperator::set_swallow_growth_rate(float rate)
{
    m_field_config.swallow_growth_rate = rate;
    invalidate();
}

void GpuFieldOperator::set_swallow_max_size(float size)
{
    m_field_config.swallow_max_size = size;
    invalidate();
}

void GpuFieldOperator::set_swallow_dim_factor(float factor)
{
    m_field_config.swallow_dim_factor = factor;
    invalidate();
}

void GpuFieldOperator::set_cross_cluster(bool enabled)
{
    m_field_config.cross_cluster = enabled;
    invalidate();
}

void GpuFieldOperator::set_spawn_density_threshold(float threshold)
{
    m_field_config.spawn_density_threshold = threshold;
    invalidate();
}

std::optional<std::pair<uint32_t, uint32_t>>
GpuFieldOperator::resolve_target(FieldTarget target) const
{
    const auto modality = target_modality(target);
    if (!modality.has_value())
        return std::nullopt;

    const auto it = std::ranges::find_if(m_layout.attributes,
        [&modality](const Kakshya::VertexAttributeLayout& a) {
            return a.component_modality == *modality;
        });

    if (it == m_layout.attributes.end())
        return std::nullopt;

    if (it->offset_in_vertex % 4 != 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator: attribute '{}' sits at byte offset {}, which is "
            "not word-aligned and cannot be addressed as a float index.",
            it->name, it->offset_in_vertex);
        return std::nullopt;
    }

    return std::make_pair(it->offset_in_vertex / 4, modality_components(*modality));
}

bool GpuFieldOperator::accept(
    FieldTarget target,
    const Kinesis::FieldSource& source,
    uint32_t components)
{
    if (m_stride_words == 0) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: operator is inert, its layout stride was "
            "rejected at construction");
        return false;
    }

    if (!any_flag(target)) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: empty target mask");
        return false;
    }

    if (!source.valid()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: field '{}' has no usable shader half. A "
            "DualField lambda must carry a trailing return type.",
            source.name);
        return false;
    }

    for (FieldTarget bit : Kinesis::k_field_targets) {
        if (!has_flag(target, bit))
            continue;

        const auto slot = resolve_target(bit);
        if (!slot.has_value()) {
            MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                "GpuFieldOperator::bind: target bit {:#x} is not present in the "
                "supplied vertex layout. Nothing was bound.",
                static_cast<uint16_t>(bit));
            return false;
        }

        if (slot->second != components) {
            MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                "GpuFieldOperator::bind: target bit {:#x} takes {} components, "
                "field '{}' produces {}. Nothing was bound.",
                static_cast<uint16_t>(bit), slot->second, source.name, components);
            return false;
        }
    }

    const auto clash = std::ranges::find_if(m_bindings,
        [&source](const Binding& b) {
            return b.source.name == source.name && b.source.body != source.body;
        });

    if (clash != m_bindings.end()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: a different field named '{}' is already "
            "bound. Emitted function names must be unique; rename one field.",
            source.name);
        return false;
    }

    return true;
}

void GpuFieldOperator::store(
    FieldTarget target,
    const Kinesis::FieldSource& source,
    uint32_t components,
    bool temporal,
    std::optional<uint32_t> cluster)
{
    m_bindings.push_back({
        .targets = target,
        .source = source,
        .components = components,
        .temporal = temporal,
        .cluster = cluster,
    });
    invalidate();
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualVectorField& field,
    std::optional<uint32_t> cluster)
{
    if (any_flag(target & ~Kinesis::k_vector_targets)) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: mask {:#x} reaches bits {:#x} that a "
            "three-component field cannot drive. Nothing was bound.",
            static_cast<uint16_t>(target),
            static_cast<uint16_t>(target & ~Kinesis::k_vector_targets));
        return;
    }

    if (!accept(target, field.source, 3))
        return;

    store(target, field.source, 3, false, cluster);
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualSpatialField& field,
    std::optional<uint32_t> cluster)
{
    if (target != FieldTarget::SCALAR) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: a scalar field binds only to SCALAR, got "
            "mask {:#x}",
            static_cast<uint16_t>(target));
        return;
    }

    if (!accept(target, field.source, 1))
        return;

    store(target, field.source, 1, false, cluster);
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualUVField& field,
    std::optional<uint32_t> cluster)
{
    if (target != FieldTarget::UV) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: a two-component field binds only to UV, got "
            "mask {:#x}",
            static_cast<uint16_t>(target));
        return;
    }

    if (!accept(target, field.source, 2))
        return;

    store(target, field.source, 2, false, cluster);
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::TemporalVectorField& field,
    std::optional<uint32_t> cluster)
{
    if (any_flag(target & ~Kinesis::k_vector_targets)) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: mask {:#x} reaches bits {:#x} that a "
            "three-component field cannot drive. Nothing was bound.",
            static_cast<uint16_t>(target),
            static_cast<uint16_t>(target & ~Kinesis::k_vector_targets));
        return;
    }

    if (!accept(target, field.source, 3))
        return;

    store(target, field.source, 3, true, cluster);
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::TemporalSpatialField& field,
    std::optional<uint32_t> cluster)
{
    if (target != FieldTarget::SCALAR) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: a scalar field binds only to SCALAR, got "
            "mask {:#x}",
            static_cast<uint16_t>(target));
        return;
    }

    if (!accept(target, field.source, 1))
        return;

    store(target, field.source, 1, true, cluster);
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::TemporalUVField& field,
    std::optional<uint32_t> cluster)
{
    if (target != FieldTarget::UV) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator::bind: a two-component field binds only to UV, got "
            "mask {:#x}",
            static_cast<uint16_t>(target));
        return;
    }

    if (!accept(target, field.source, 2))
        return;

    store(target, field.source, 2, true, cluster);
}

void GpuFieldOperator::unbind(FieldTarget target)
{
    for (auto& b : m_bindings)
        b.targets &= ~target;

    std::erase_if(m_bindings,
        [](const Binding& b) { return !any_flag(b.targets); });

    invalidate();
}

void GpuFieldOperator::set_vertex_binding(uint32_t binding)
{
    if (m_vertex_binding == binding)
        return;
    m_vertex_binding = binding;
    invalidate();
}

void GpuFieldOperator::set_workgroup_size(uint32_t x)
{
    if (x == 0 || m_workgroup_size == x)
        return;
    m_workgroup_size = x;
    invalidate();
}

void GpuFieldOperator::invalidate()
{
    m_spec_cache.reset();
    ++m_revision;
}

void GpuFieldOperator::process(float /*dt*/)
{
}

void GpuFieldOperator::set_parameter(std::string_view param, double value)
{
    MF_RT_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GpuFieldOperator: unknown parameter '{}' ({})", param, value);
}

bool GpuFieldOperator::needs_cluster_id() const
{
    return std::ranges::any_of(m_bindings,
        [](const Binding& b) { return b.cluster.has_value(); });
}

std::optional<double> GpuFieldOperator::query_state(std::string_view query) const
{
    if (query == "binding_count")
        return static_cast<double>(m_bindings.size());
    if (query == "stride_words")
        return static_cast<double>(m_stride_words);
    return std::nullopt;
}

std::optional<Portal::Graphics::ShaderSpec> GpuFieldOperator::build_spec() const
{
    if (m_spec_cache.has_value())
        return m_spec_cache;

    if (m_bindings.empty())
        return std::nullopt;

    const auto position = resolve_target(Kinesis::FieldTarget::POSITION);
    if (!position.has_value()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GpuFieldOperator: layout carries no position attribute. Fields are "
            "functions of position and cannot be evaluated without one.");
        return std::nullopt;
    }

    const bool needs_cluster = needs_cluster_id();

    Portal::Graphics::ShaderSpec::Assemble assemble;
    assemble.start_binding(m_vertex_binding)
        .ssbo("vertices", Portal::Graphics::BindingDirection::InOut,
            Kakshya::GpuDataFormat::FLOAT32);

    if (needs_cluster) {
        assemble.ssbo("cluster_id", Portal::Graphics::BindingDirection::Input,
            Kakshya::GpuDataFormat::UINT32);
    }

    assemble.pc("first_vertex", Kakshya::GpuDataFormat::UINT32)
        .pc("vertex_count", Kakshya::GpuDataFormat::UINT32)
        .pc("stride_words", Kakshya::GpuDataFormat::UINT32)
        .pc("time", Kakshya::GpuDataFormat::FLOAT32)
        .workgroup(m_workgroup_size);

    std::vector<std::string_view> emitted;
    for (const auto& b : m_bindings) {
        const auto seen = std::ranges::find(emitted, b.source.name);
        if (seen != emitted.end())
            continue;
        emitted.push_back(b.source.name);
        assemble.function(b.source.return_type, b.source.name,
            b.source.params, b.source.body);
    }

    const uint32_t pw = position->first;
    std::string body;
    body += "    if (i >= vertex_count) { return; }\n";
    body += "    uint b = (first_vertex + i) * stride_words;\n";
    body += "    vec3 p = vec3(vertices[b + " + word(pw)
        + "], vertices[b + " + word(pw + 1)
        + "], vertices[b + " + word(pw + 2) + "]);\n";

    if (needs_cluster) {
        body += "    uint my_cluster = cluster_id[first_vertex + i];\n";
    }

    for (Kinesis::FieldTarget t : Kinesis::k_field_targets) {
        const bool touched = std::ranges::any_of(m_bindings,
            [t](const Binding& b) { return has_flag(b.targets, t); });
        if (!touched)
            continue;

        const auto slot = resolve_target(t);
        if (!slot.has_value())
            continue;

        const auto [w, components] = *slot;
        const std::string acc = std::string("acc_") + target_name(t);

        body += "\n    " + std::string(glsl_type(components)) + " " + acc
            + " = " + glsl_zero(components) + ";\n";

        for (const auto& b : m_bindings) {
            if (!has_flag(b.targets, t))
                continue;

            const std::string call = std::string(b.source.name)
                + (b.temporal ? "(p, time)" : "(p)");

            if (b.cluster.has_value()) {
                body += "    if (my_cluster == " + word(*b.cluster) + ") { "
                    + acc + " += " + call + "; }\n";
            } else {
                body += "    " + acc + " += " + call + ";\n";
            }
        }

        if (t == Kinesis::FieldTarget::NORMAL || t == Kinesis::FieldTarget::TANGENT) {
            const std::string len = std::string("len_") + target_name(t);
            body += "    float " + len + " = length(" + acc + ");\n";
            body += "    " + acc + " = " + len + " > 1e-6f ? " + acc + " / "
                + len + " : " + acc + ";\n";
        }

        const char* assign = target_accumulates(t) ? " += " : " = ";
        for (uint32_t k = 0; k < components; ++k) {
            body += "    vertices[b + " + word(w + k) + "]" + assign
                + acc + (components == 1 ? "" : k_swizzle[k]) + ";\n";
        }
    }

    std::vector<std::string> param_names { "vertices" };
    if (needs_cluster) {
        param_names.emplace_back("cluster_id");
    }
    param_names.insert(param_names.end(),
        { "first_vertex", "vertex_count", "stride_words", "time", "i" });

    assemble.kernel(Portal::Graphics::KernelSource {
        .raw = {},
        .param_names = std::move(param_names),
        .body = std::move(body),
    });

    m_spec_cache = assemble.build();
    return m_spec_cache;
}

} // namespace MayaFlux::Nodes::Network
