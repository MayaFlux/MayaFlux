#pragma once

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file Layers.hpp
 * @brief Descriptions of the layers the tensor kernels run, and their output sizes.
 */

namespace MayaFlux::Yantra::TensorKernels {

/**
 * @enum Activation
 * @brief Output activation of a layer.
 */
enum class Activation : uint8_t {
    None,
    Relu,
    Sigmoid
};

/**
 * @struct Conv1d
 * @brief One convolution layer over [batch, c_in, t_in], with weights [c_out, c_in, kernel].
 *
 * The input is zero padded by pad on both sides, tap k reads dilation * k
 * samples past the window start, and windows advance by stride. relu_input
 * applies ReLU to the input before the weights.
 */
struct Conv1d {
    uint32_t c_in {};
    uint32_t c_out {};
    uint32_t kernel {};
    uint32_t stride { 1 };
    uint32_t pad { 0 };
    uint32_t dilation { 1 };
    bool relu_input { false };
    Activation activation { Activation::None };
};

namespace detail {

    [[noreturn]] inline void refuse(std::string_view message)
    {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "{}", message);
    }

}

/**
 * @brief Output length of a convolution, 0 when the window does not fit the padded input.
 * @param layer The layer. Kernel, stride and dilation must be at least 1.
 * @param t_in Input length.
 * @throws std::invalid_argument When kernel, stride or dilation is 0.
 */
[[nodiscard]] inline uint32_t conv1d_length(const Conv1d& layer, uint32_t t_in)
{
    if (layer.kernel == 0 || layer.stride == 0 || layer.dilation == 0) {
        detail::refuse("Conv1d needs a kernel, stride and dilation of at least 1");
    }
    const uint64_t span = static_cast<uint64_t>(layer.dilation) * (layer.kernel - 1U) + 1U;
    const uint64_t padded = static_cast<uint64_t>(t_in) + 2ULL * layer.pad;
    return padded < span ? 0U : static_cast<uint32_t>((padded - span) / layer.stride + 1U);
}

/**
 * @brief Length of one frame nn_frame_audio writes.
 */
[[nodiscard]] constexpr uint32_t frame_audio_length(uint32_t chunk_size, uint32_t context, uint32_t pad)
{
    return context + chunk_size + pad;
}

} // namespace MayaFlux::Yantra::TensorKernels
