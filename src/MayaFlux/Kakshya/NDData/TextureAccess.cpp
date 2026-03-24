#include "TextureAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

//==============================================================================
// TextureAccess helpers
//==============================================================================

size_t TextureAccess::bytes_per_texel() const noexcept
{
    switch (format) {
    case GpuDataFormat::UINT8:
        return 1;
    case GpuDataFormat::UINT16:
        return 2;
    case GpuDataFormat::UINT32:
    case GpuDataFormat::INT32:
    case GpuDataFormat::FLOAT32:
        return 4;
    case GpuDataFormat::VEC2_F32:
        return 8;
    case GpuDataFormat::VEC3_F32:
        return 12;
    case GpuDataFormat::VEC4_F32:
        return 16;
    case GpuDataFormat::FLOAT64:
        return 8;
    case GpuDataFormat::VEC2_F64:
        return 16;
    case GpuDataFormat::VEC3_F64:
        return 24;
    case GpuDataFormat::VEC4_F64:
        return 32;
    default:
        return 0;
    }
}

uint32_t TextureAccess::channel_count() const noexcept
{
    switch (format) {
    case GpuDataFormat::UINT8:
    case GpuDataFormat::UINT16:
    case GpuDataFormat::UINT32:
    case GpuDataFormat::INT32:
    case GpuDataFormat::FLOAT32:
    case GpuDataFormat::FLOAT64:
        return 1;
    case GpuDataFormat::VEC2_F32:
    case GpuDataFormat::VEC2_F64:
        return 2;
    case GpuDataFormat::VEC3_F32:
    case GpuDataFormat::VEC3_F64:
        return 3;
    case GpuDataFormat::VEC4_F32:
    case GpuDataFormat::VEC4_F64:
        return 4;
    default:
        return 0;
    }
}

//==============================================================================
// as_texture_access
//==============================================================================

std::optional<TextureAccess> as_texture_access(const DataVariant& variant)
{
    return std::visit([](const auto& vec) -> std::optional<TextureAccess> {
        using T = typename std::decay_t<decltype(vec)>::value_type;

        if (vec.empty()) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_texture_access: empty variant");
            return std::nullopt;
        }

        // ---- rejected types ------------------------------------------------

        if constexpr (std::is_same_v<T, std::complex<double>>) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_texture_access: complex<double> (RG64F) is not a sampled "
                "image format in Vulkan. Use complex<float> for RG32F.");
            return std::nullopt;
        }

        if constexpr (std::is_same_v<T, glm::mat4>) {
            MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_texture_access: mat4 layout is ambiguous for texel upload. "
                "Unpack each column to a vec4 and use vector<glm::vec4>.");
            return std::nullopt;
        }

        // ---- zero-copy direct mappings ------------------------------------

        if constexpr (std::is_same_v<T, uint8_t>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::UINT8
            };
        }

        if constexpr (std::is_same_v<T, uint16_t>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::UINT16
            };
        }

        if constexpr (std::is_same_v<T, uint32_t>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::UINT32
            };
        }

        if constexpr (std::is_same_v<T, float>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::FLOAT32
            };
        }

        if constexpr (std::is_same_v<T, glm::vec2>
            || std::is_same_v<T, std::complex<float>>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::VEC2_F32
            };
        }

        if constexpr (std::is_same_v<T, glm::vec4>) {
            return TextureAccess {
                .data_ptr = vec.data(),
                .byte_count = vec.size() * sizeof(T),
                .format = GpuDataFormat::VEC4_F32
            };
        }

        // ---- narrowing: double → float -------------------------------------

        if constexpr (std::is_same_v<T, double>) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_texture_access: double narrowed to float for texel upload. "
                "Precision beyond float range is lost.");
            TextureAccess acc;
            acc.format = GpuDataFormat::FLOAT32;
            acc.conversion_buffer.resize(vec.size() * sizeof(float));
            auto* dst = reinterpret_cast<float*>(acc.conversion_buffer.data());
            for (size_t i = 0; i < vec.size(); ++i) {
                dst[i] = static_cast<float>(vec[i]);
            }
            acc.data_ptr = acc.conversion_buffer.data();
            acc.byte_count = acc.conversion_buffer.size();
            return acc;
        }

        // ---- promotion: vec3 → vec4 (W = 0) --------------------------------

        if constexpr (std::is_same_v<T, glm::vec3>) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "as_texture_access: vec3 promoted to vec4 (W=0). "
                "RGB32F is not a universally-supported sampled image format in Vulkan.");
            TextureAccess acc;
            acc.format = GpuDataFormat::VEC4_F32;
            acc.conversion_buffer.resize(vec.size() * sizeof(glm::vec4));
            auto* dst = reinterpret_cast<glm::vec4*>(acc.conversion_buffer.data());
            for (size_t i = 0; i < vec.size(); ++i) {
                dst[i] = glm::vec4(vec[i], 0.0F);
            }
            acc.data_ptr = acc.conversion_buffer.data();
            acc.byte_count = acc.conversion_buffer.size();
            return acc;
        }

        // ---- unreachable for current DataVariant definition ----------------
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_texture_access: unhandled variant type {}",
            typeid(T).name());
        return std::nullopt;
    },
        variant);
}

} // namespace MayaFlux::Kakshya
