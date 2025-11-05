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

} // namespace MayaFlux::Nodes
