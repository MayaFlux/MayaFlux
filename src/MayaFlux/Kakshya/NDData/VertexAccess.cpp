#include "VertexAccess.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

namespace {

    constexpr size_t k_vertex_bytes = 68;

    // Canonical field layout: vec3 pos(12) | vec4 color(16) | float scalar(4) | vec2 uv(8) | vec3 normal(12) | vec4 tangent(16)
    // Channel slot -> (offset, size)
    constexpr std::array<std::pair<size_t, size_t>, 6> k_channel_layout { {
        { 0, 12 }, // vec3  position
        { 12, 16 }, // vec4  color
        { 28, 4 }, // float scalar
        { 32, 8 }, // vec2  uv
        { 40, 12 }, // vec3  normal
        { 52, 16 }, // vec4  tangent
    } };

    /**
     * Returns (data pointer, element count, element size in bytes) for a channel variant.
     * Element size is the sizeof of the stored type, not the canonical field size.
     */
    struct ChannelView {
        const uint8_t* ptr;
        size_t count;
        size_t element_bytes;
    };

    ChannelView channel_view(const DataVariant& v)
    {
        return std::visit([](const auto& vec) -> ChannelView {
            using V = typename std::decay_t<decltype(vec)>::value_type;
            if (vec.empty())
                return { .ptr = nullptr, .count = 0, .element_bytes = sizeof(V) };
            return {
                reinterpret_cast<const uint8_t*>(vec.data()),
                vec.size(),
                sizeof(V)
            };
        },
            v);
    }

    void assemble_vertices(
        std::span<const DataVariant> channels,
        size_t count,
        const VertexAccessConfig& cfg,
        float slot24_default,
        std::byte* dst)
    {
        const glm::vec4 default_tangent(cfg.default_tangent, 1.0F);
        const void* defaults[6] = {
            nullptr,
            &cfg.default_color,
            &slot24_default,
            &cfg.default_uv,
            &cfg.default_normal,
            &default_tangent,
        };

        for (size_t i = 0; i < count; ++i) {
            std::byte* v = dst + i * k_vertex_bytes;
            for (size_t s = 0; s < 6; ++s) {
                auto [dst_off, dst_sz] = k_channel_layout[s];
                if (s < channels.size()) {
                    auto cv = channel_view(channels[s]);
                    // copy min(src element size, dst field size) bytes, zero-pad remainder
                    const size_t copy_sz = std::min(cv.element_bytes, dst_sz);
                    std::memcpy(v + dst_off, cv.ptr + i * cv.element_bytes, copy_sz);
                    if (copy_sz < dst_sz) {
                        std::memset(v + dst_off + copy_sz, 0, dst_sz - copy_sz);
                        if ((s == 1 || s == 5) && copy_sz == sizeof(glm::vec3)) {
                            const float opaque = 1.0F;
                            std::memcpy(v + dst_off + copy_sz, &opaque, sizeof(float));
                        }
                    }
                } else {
                    std::memcpy(v + dst_off, defaults[s], dst_sz);
                }
            }
        }
    }

} // namespace

std::optional<VertexAccess> as_vertex_access(
    std::span<const DataVariant> channels,
    const VertexAccessConfig& config)
{
    return as_point_vertex_access(channels, config);
}

std::optional<VertexAccess> as_point_vertex_access(
    std::span<const DataVariant> channels,
    const VertexAccessConfig& config)
{
    if (channels.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_point_vertex_access: no channels supplied");
        return std::nullopt;
    }

    // Zero-copy path: single uint8_t channel of N*68 bytes is already interleaved.
    if (channels.size() == 1) {
        if (const auto* b = std::get_if<std::vector<uint8_t>>(&channels[0])) {
            if (b->size() % k_vertex_bytes != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_point_vertex_access: uint8_t byte count {} not a multiple of 68",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / k_vertex_bytes);
            auto layout = VertexLayout::for_points();
            layout.vertex_count = count;
            return VertexAccess { .data_ptr = b->data(), .byte_count = b->size(), .layout = layout };
        }
    }

    const size_t count = channel_view(channels[0]).count;
    if (count == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_point_vertex_access: position channel (slot 0) is empty or unsupported");
        return std::nullopt;
    }

    VertexAccess va;
    va.conversion_buffer.resize(count * k_vertex_bytes);
    assemble_vertices(channels, count, config, config.default_size,
        va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_points();
    va.layout.vertex_count = static_cast<uint32_t>(count);
    return va;
}

std::optional<VertexAccess> as_line_vertex_access(
    std::span<const DataVariant> channels,
    const VertexAccessConfig& config)
{
    if (channels.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_line_vertex_access: no channels supplied");
        return std::nullopt;
    }

    if (channels.size() == 1) {
        if (const auto* b = std::get_if<std::vector<uint8_t>>(&channels[0])) {
            if (b->size() % k_vertex_bytes != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_line_vertex_access: uint8_t byte count {} not a multiple of 68",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / k_vertex_bytes);
            auto layout = VertexLayout::for_lines();
            layout.vertex_count = count;
            return VertexAccess { .data_ptr = b->data(), .byte_count = b->size(), .layout = layout };
        }
    }

    const size_t count = channel_view(channels[0]).count;
    if (count == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_line_vertex_access: position channel (slot 0) is empty or unsupported");
        return std::nullopt;
    }

    VertexAccess va;
    va.conversion_buffer.resize(count * k_vertex_bytes);
    assemble_vertices(channels, count, config, config.default_thickness,
        va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_lines();
    va.layout.vertex_count = static_cast<uint32_t>(count);
    return va;
}

std::optional<VertexAccess> as_mesh_vertex_access(
    std::span<const DataVariant> channels,
    const VertexAccessConfig& config)
{
    if (channels.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_vertex_access: no channels supplied");
        return std::nullopt;
    }

    if (channels.size() == 1) {
        if (const auto* b = std::get_if<std::vector<uint8_t>>(&channels[0])) {
            if (b->size() % k_vertex_bytes != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_mesh_vertex_access: uint8_t byte count {} not a multiple of 68",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / k_vertex_bytes);
            auto layout = VertexLayout::for_meshes();
            layout.vertex_count = count;
            return VertexAccess { .data_ptr = b->data(), .byte_count = b->size(), .layout = layout };
        }
    }

    const size_t count = channel_view(channels[0]).count;
    if (count == 0) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_mesh_vertex_access: position channel (slot 0) is empty or unsupported");
        return std::nullopt;
    }

    VertexAccess va;
    va.conversion_buffer.resize(count * k_vertex_bytes);
    assemble_vertices(channels, count, config, config.default_weight,
        va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_meshes();
    va.layout.vertex_count = static_cast<uint32_t>(count);
    return va;
}

} // namespace MayaFlux::Kakshya
