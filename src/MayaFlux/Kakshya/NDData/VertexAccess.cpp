#include "VertexAccess.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

namespace {

    // Canonical field layout: vec3 pos(12) | vec3 color(12) | float scalar(4) | vec2 uv(8) | vec3 normal(12) | vec3 tangent(12)
    // Channel slot -> (offset, size)
    constexpr std::array<std::pair<size_t, size_t>, 6> k_channel_layout { {
        { 0, 12 }, // vec3  position
        { 12, 12 }, // vec3  color
        { 24, 4 }, // float scalar
        { 28, 8 }, // vec2  uv
        { 36, 12 }, // vec3  normal
        { 48, 12 }, // vec3  tangent
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
        const void* defaults[6] = {
            nullptr,
            &cfg.default_color,
            &slot24_default,
            &cfg.default_uv,
            &cfg.default_normal,
            &cfg.default_tangent,
        };

        for (size_t i = 0; i < count; ++i) {
            std::byte* v = dst + i * 60;
            for (size_t s = 0; s < 6; ++s) {
                auto [dst_off, dst_sz] = k_channel_layout[s];
                if (s < channels.size()) {
                    auto cv = channel_view(channels[s]);
                    // copy min(src element size, dst field size) bytes, zero-pad remainder
                    const size_t copy_sz = std::min(cv.element_bytes, dst_sz);
                    std::memcpy(v + dst_off, cv.ptr + i * cv.element_bytes, copy_sz);
                    if (copy_sz < dst_sz) {
                        std::memset(v + dst_off + copy_sz, 0, dst_sz - copy_sz);
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

    // Zero-copy path: single uint8_t channel of N*60 bytes is already interleaved.
    if (channels.size() == 1) {
        if (const auto* b = std::get_if<std::vector<uint8_t>>(&channels[0])) {
            if (b->size() % 60 != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_point_vertex_access: uint8_t byte count {} not a multiple of 60",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / 60);
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
    va.conversion_buffer.resize(count * 60);
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
            if (b->size() % 60 != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_line_vertex_access: uint8_t byte count {} not a multiple of 60",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / 60);
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
    va.conversion_buffer.resize(count * 60);
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
            if (b->size() % 60 != 0) {
                MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_mesh_vertex_access: uint8_t byte count {} not a multiple of 60",
                    b->size());
                return std::nullopt;
            }
            const auto count = static_cast<uint32_t>(b->size() / 60);
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
    va.conversion_buffer.resize(count * 60);
    assemble_vertices(channels, count, config, config.default_weight,
        va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_meshes();
    va.layout.vertex_count = static_cast<uint32_t>(count);
    return va;
}

} // namespace MayaFlux::Kakshya
