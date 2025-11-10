#pragma once

namespace MayaFlux::Nodes {

/**
 * @class GpuVectorData
 * @brief GPU-uploadable 1D array data interface
 *
 * Provides contiguous float array access for GPU descriptor binding.
 * Use for: FFT spectrums, audio buffers, particle data, filter coefficients.
 */
class MAYAFLUX_API GpuVectorData {
protected:
    std::span<const float> m_gpu_data;

public:
    GpuVectorData() = default;
    explicit GpuVectorData(std::span<const float> data)
        : m_gpu_data(data)
    {
    }

    [[nodiscard]] std::span<const float> gpu_data() const { return m_gpu_data; }
    [[nodiscard]] size_t gpu_data_size() const { return m_gpu_data.size(); }
    [[nodiscard]] bool has_gpu_data() const { return !m_gpu_data.empty(); }
};

/**
 * @class GpuMatrixData
 * @brief GPU-uploadable 2D grid data interface
 *
 * Provides 2D texture data access for GPU descriptor binding.
 * Use for: textures, spectrograms, 2D visualizations.
 */
class MAYAFLUX_API GpuMatrixData {
protected:
    std::span<const float> m_gpu_data;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

public:
    GpuMatrixData() = default;
    GpuMatrixData(std::span<const float> data, uint32_t w, uint32_t h)
        : m_gpu_data(data)
        , m_width(w)
        , m_height(h)
    {
    }

    [[nodiscard]] std::span<const float> gpu_data() const { return m_gpu_data; }
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }
    [[nodiscard]] bool has_gpu_data() const { return !m_gpu_data.empty(); }
};

/**
 * @class GpuStructuredData
 * @brief GPU-uploadable structured data (arrays of POD structs)
 *
 * Provides raw byte access for arrays of POD structures.
 * Use for: particle systems, instance data, custom vertex attributes.
 */
class MAYAFLUX_API GpuStructuredData {
protected:
    std::span<const uint8_t> m_gpu_data;
    size_t m_element_size = 0;
    size_t m_element_count = 0;

public:
    GpuStructuredData() = default;
    GpuStructuredData(std::span<const uint8_t> data, size_t elem_size, size_t elem_count)
        : m_gpu_data(data)
        , m_element_size(elem_size)
        , m_element_count(elem_count)
    {
    }

    std::span<const uint8_t> gpu_data() const { return m_gpu_data; }
    size_t element_size() const { return m_element_size; }
    size_t element_count() const { return m_element_count; }
    bool has_gpu_data() const { return !m_gpu_data.empty(); }
};

} // namespace MayaFlux::Nodes
