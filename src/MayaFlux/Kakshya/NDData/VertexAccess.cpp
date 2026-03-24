#include "VertexAccess.hpp"

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

namespace {

    void floats_to_waveform(std::span<const float> src, std::byte* dst)
    {
        const size_t count = src.size();
        auto* out = reinterpret_cast<glm::vec3*>(dst);
        const float inv = (count > 1) ? (2.0F / static_cast<float>(count - 1)) : 0.0F;

        for (size_t i = 0; i < count; ++i) {
            out[i] = glm::vec3(static_cast<float>(i) * inv - 1.0F, src[i], 0.0F);
        }
    }

    VertexLayout position_only_layout(uint32_t count)
    {
        VertexLayout layout;
        layout.vertex_count = count;
        layout.stride_bytes = sizeof(glm::vec3);
        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });
        return layout;
    }

    VertexLayout position_2d_layout(uint32_t count)
    {
        VertexLayout layout;
        layout.vertex_count = count;
        layout.stride_bytes = sizeof(glm::vec2);
        layout.attributes.push_back({ .component_modality = DataModality::TEXTURE_COORDS_2D,
            .offset_in_vertex = 0,
            .name = "position2d" });
        return layout;
    }

    VertexLayout position_w_layout(uint32_t count)
    {
        VertexLayout layout;
        layout.vertex_count = count;
        layout.stride_bytes = sizeof(glm::vec4);
        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_COLORS_RGBA,
            .offset_in_vertex = 0,
            .name = "position_w" });
        return layout;
    }

    VertexLayout waveform_layout(uint32_t count)
    {
        VertexLayout layout;
        layout.vertex_count = count;
        layout.stride_bytes = sizeof(glm::vec3);
        layout.attributes.push_back({ .component_modality = DataModality::VERTEX_POSITIONS_3D,
            .offset_in_vertex = 0,
            .name = "position" });
        return layout;
    }

    // =========================================================================
    // Shared interleaved write helpers
    // =========================================================================

    /**
     * Write N point vertices into dst.
     * positions must have exactly count elements.
     * dst must be pre-sized to count * 28 bytes.
     */
    void write_point_vertices(
        std::span<const glm::vec3> positions,
        const VertexAccessConfig& cfg,
        std::byte* dst)
    {
        for (size_t i = 0; i < positions.size(); ++i) {
            std::byte* v = dst + i * 28;
            std::memcpy(v, &positions[i], 12);
            std::memcpy(v + 12, &cfg.default_color, 12);
            std::memcpy(v + 24, &cfg.default_size, 4);
        }
    }

    /**
     * Write N line vertices into dst.
     * positions must have exactly count elements.
     * dst must be pre-sized to count * 36 bytes.
     */
    void write_line_vertices(
        std::span<const glm::vec3> positions,
        const VertexAccessConfig& cfg,
        std::byte* dst)
    {
        for (size_t i = 0; i < positions.size(); ++i) {
            std::byte* v = dst + i * 36;
            std::memcpy(v, &positions[i], 12);
            std::memcpy(v + 12, &cfg.default_color, 12);
            std::memcpy(v + 24, &cfg.default_thickness, 4);
            std::memcpy(v + 28, &cfg.default_uv, 8);
        }
    }

    // =========================================================================
    // Shared position extraction — reuses existing waveform + DataConverter paths
    // =========================================================================

    /**
     * Extract positions from any DataVariant into a vector<vec3>.
     * GLM types are mapped directly; scalar/integer/complex go through
     * the same waveform path as as_vertex_access().
     * Returns empty on rejected types (complex<double>, mat4).
     */
    std::vector<glm::vec3> extract_positions(const DataVariant& variant)
    {
        return std::visit([variant](const auto& vec) -> std::vector<glm::vec3> {
            using V = typename std::decay_t<decltype(vec)>::value_type;
            const size_t count = vec.size();

            if (count == 0) {
                return {};
            }

            if constexpr (std::is_same_v<V, glm::vec3>) {
                return std::vector<glm::vec3>(vec.begin(), vec.end());
            }

            if constexpr (std::is_same_v<V, glm::vec2>) {
                std::vector<glm::vec3> out(count);
                for (size_t i = 0; i < count; ++i) {
                    out[i] = glm::vec3(vec[i].x, vec[i].y, 0.0F);
                }
                return out;
            }

            if constexpr (std::is_same_v<V, glm::vec4>) {
                std::vector<glm::vec3> out(count);
                for (size_t i = 0; i < count; ++i) {
                    out[i] = glm::vec3(vec[i].x, vec[i].y, vec[i].z);
                }
                return out;
            }

            if constexpr (DecimalData<V> || IntegerData<V>) {
                std::vector<float> floats;
                extract_from_variant<float>(variant, floats);

                const float inv = count > 1
                    ? 2.0F / static_cast<float>(count - 1)
                    : 0.0F;

                std::vector<glm::vec3> out(count);
                for (size_t i = 0; i < count; ++i) {
                    out[i] = glm::vec3(static_cast<float>(i) * inv - 1.0F,
                        floats[i], 0.0F);
                }
                return out;
            }

            if constexpr (std::is_same_v<V, std::complex<float>>) {
                std::vector<float> magnitudes;
                extract_from_variant<float>(variant, magnitudes,
                    ComplexConversionStrategy::MAGNITUDE);

                constexpr float inv_pi = std::numbers::inv_pi_v<float>;
                const float inv = count > 1
                    ? 2.0F / static_cast<float>(count - 1)
                    : 0.0F;

                std::vector<glm::vec3> out(count);
                for (size_t i = 0; i < count; ++i) {
                    out[i] = glm::vec3(static_cast<float>(i) * inv - 1.0F,
                        magnitudes[i],
                        std::arg(vec[i]) * inv_pi);
                }
                return out;
            }

            // complex<double> and mat4 rejected
            return {};
        },
            variant);
    }

} // namespace

std::optional<VertexAccess> as_vertex_access(const DataVariant& variant)
{
    return std::visit([&variant](const auto& vec) -> std::optional<VertexAccess> {
        using V = typename std::decay_t<decltype(vec)>::value_type;
        const size_t count = vec.size();

        if (count == 0) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_vertex_access: empty variant");
            return std::nullopt;
        }

        if constexpr (std::is_same_v<V, glm::vec3>) {
            return VertexAccess { vec.data(), count * sizeof(glm::vec3),
                position_only_layout(static_cast<uint32_t>(count)) };
        }

        if constexpr (std::is_same_v<V, glm::vec2>) {
            return VertexAccess { vec.data(), count * sizeof(glm::vec2),
                position_2d_layout(static_cast<uint32_t>(count)) };
        }

        if constexpr (std::is_same_v<V, glm::vec4>) {
            return VertexAccess { vec.data(), count * sizeof(glm::vec4),
                position_w_layout(static_cast<uint32_t>(count)) };
        }

        if constexpr (DecimalData<V> || IntegerData<V>) {
            if constexpr (std::is_same_v<V, double>) {
                MF_TRACE(Journal::Component::Kakshya, Journal::Context::Runtime,
                    "as_vertex_access: double narrowed to float for vertex waveform");
            }

            std::vector<float> floats;
            extract_from_variant<float>(variant, floats);

            VertexAccess va;
            va.conversion_buffer.resize(count * sizeof(glm::vec3));
            floats_to_waveform(floats, va.conversion_buffer.data());
            va.data_ptr = va.conversion_buffer.data();
            va.byte_count = va.conversion_buffer.size();
            va.layout = waveform_layout(static_cast<uint32_t>(count));
            return va;
        }

        if constexpr (std::is_same_v<V, std::complex<float>>) {
            std::vector<float> magnitudes;
            extract_from_variant<float>(variant, magnitudes,
                ComplexConversionStrategy::MAGNITUDE);

            constexpr float inv_pi = std::numbers::inv_pi_v<float>;
            const float inv = (count > 1) ? (2.0F / static_cast<float>(count - 1)) : 0.0F;

            VertexAccess va;
            va.conversion_buffer.resize(count * sizeof(glm::vec3));
            auto* out = reinterpret_cast<glm::vec3*>(va.conversion_buffer.data());

            for (size_t i = 0; i < count; ++i) {
                out[i] = glm::vec3(
                    static_cast<float>(i) * inv - 1.0F,
                    magnitudes[i],
                    std::arg(vec[i]) * inv_pi);
            }

            va.data_ptr = va.conversion_buffer.data();
            va.byte_count = va.conversion_buffer.size();
            va.layout = waveform_layout(static_cast<uint32_t>(count));
            return va;
        }

        if constexpr (std::is_same_v<V, std::complex<double>>) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_vertex_access: complex<double> rejected: convert to "
                "complex<float> or extract components manually");
            return std::nullopt;
        }

        if constexpr (std::is_same_v<V, glm::mat4>) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_vertex_access: glm::mat4 rejected: unpack to vec4 columns first");
            return std::nullopt;
        }

        return std::nullopt;
    },
        variant);
}

std::optional<VertexAccess> as_point_vertex_access(
    const DataVariant& variant,
    const VertexAccessConfig& config)
{
    auto positions = extract_positions(variant);
    if (positions.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_point_vertex_access: unsupported or empty variant");
        return std::nullopt;
    }

    const auto count = static_cast<uint32_t>(positions.size());
    VertexAccess va;
    va.conversion_buffer.resize((size_t)count * 28);
    write_point_vertices(positions, config, va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_points();
    va.layout.vertex_count = count;
    return va;
}

std::optional<VertexAccess> as_line_vertex_access(
    const DataVariant& variant,
    const VertexAccessConfig& config)
{
    auto positions = extract_positions(variant);
    if (positions.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_line_vertex_access: unsupported or empty variant");
        return std::nullopt;
    }

    const auto count = static_cast<uint32_t>(positions.size());
    VertexAccess va;
    va.conversion_buffer.resize((size_t)count * 36);
    write_line_vertices(positions, config, va.conversion_buffer.data());
    va.data_ptr = va.conversion_buffer.data();
    va.byte_count = va.conversion_buffer.size();
    va.layout = VertexLayout::for_lines();
    va.layout.vertex_count = count;
    return va;
}

} // namespace MayaFlux::Kakshya
