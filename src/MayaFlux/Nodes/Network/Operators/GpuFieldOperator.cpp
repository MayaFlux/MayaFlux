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

GpuFieldOperator::GpuFieldOperator(Kakshya::VertexLayout layout)
    : m_layout(std::move(layout))
{
    m_push_constants.resize(sizeof(float));
    std::memcpy(m_push_constants.data(), &m_dt, sizeof(float));

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

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualVectorField& field)
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

    m_bindings.push_back({ .targets = target, .source = field.source, .components = 3 });
    invalidate();
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualSpatialField& field)
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

    m_bindings.push_back({ .targets = target, .source = field.source, .components = 1 });
    invalidate();
}

void GpuFieldOperator::bind(FieldTarget target, const Kinesis::DualUVField& field)
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

    m_bindings.push_back({ .targets = target, .source = field.source, .components = 2 });
    invalidate();
}

void GpuFieldOperator::unbind(FieldTarget target)
{
    for (auto& b : m_bindings)
        b.targets &= ~target;

    std::erase_if(m_bindings,
        [](const Binding& b) { return !any_flag(b.targets); });

    invalidate();
}

void GpuFieldOperator::set_mode(FieldMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
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
}

void GpuFieldOperator::process(float dt)
{
    m_dt = dt;
    std::memcpy(m_push_constants.data(), &m_dt, sizeof(float));
}

std::span<const uint8_t> GpuFieldOperator::push_constants() const
{
    return { m_push_constants.data(), m_push_constants.size() };
}

void GpuFieldOperator::set_parameter(std::string_view param, double value)
{
    MF_RT_TRACE(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "GpuFieldOperator: unknown parameter '{}' ({})", param, value);
}

std::optional<double> GpuFieldOperator::query_state(std::string_view query) const
{
    if (query == "dt")
        return static_cast<double>(m_dt);
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

    if (m_mode == Kinesis::FieldMode::ABSOLUTE) {
        for (const auto& b : m_bindings) {
            if (has_flag(b.targets, Kinesis::FieldTarget::POSITION)) {
                MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                    "GpuFieldOperator: ABSOLUTE mode with a POSITION binding requires "
                    "a reference copy of the vertex records, which this operator does "
                    "not allocate. Use ACCUMULATE, or drive POSITION from a separate "
                    "operator that owns reference state.");
                return std::nullopt;
            }
        }
    }

    Portal::Graphics::ShaderSpec::Assemble assemble;
    assemble.start_binding(m_vertex_binding)
        .ssbo("vertices", Portal::Graphics::BindingDirection::InOut,
            Kakshya::GpuDataFormat::FLOAT32)
        .pc("dt")
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
    body += "    uint b = i * " + word(m_stride_words) + ";\n";
    body += "    vec3 p = vec3(vertices[b + " + word(pw)
        + "], vertices[b + " + word(pw + 1)
        + "], vertices[b + " + word(pw + 2) + "]);\n";

    const bool scaled = m_mode == Kinesis::FieldMode::ACCUMULATE;
    const char* assign = scaled ? " += " : " = ";

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
            if (has_flag(b.targets, t))
                body += "    " + acc + " += " + b.source.name + "(p);\n";
        }

        if (t == Kinesis::FieldTarget::NORMAL || t == Kinesis::FieldTarget::TANGENT) {
            const std::string len = std::string("len_") + target_name(t);
            body += "    float " + len + " = length(" + acc + ");\n";
            body += "    " + acc + " = " + len + " > 1e-6f ? " + acc + " / "
                + len + " : " + acc + ";\n";
        }

        const std::string rhs = scaled ? (acc + " * dt") : acc;
        for (uint32_t k = 0; k < components; ++k) {
            body += "    vertices[b + " + word(w + k) + "]" + assign
                + (components == 1 ? rhs : "(" + rhs + ")" + k_swizzle[k]) + ";\n";
        }
    }

    assemble.kernel(Portal::Graphics::KernelSource {
        .raw = {},
        .param_names = { "vertices", "dt", "i" },
        .body = std::move(body),
    });

    m_spec_cache = assemble.build();
    return m_spec_cache;
}

} // namespace MayaFlux::Nodes::Network
