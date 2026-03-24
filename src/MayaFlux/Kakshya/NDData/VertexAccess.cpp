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
                MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
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

} // namespace MayaFlux::Kakshya
