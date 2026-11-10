#pragma once

/**
 * @file Layouts.hpp
 * @brief The push constant layouts of the tensor kernels, their flag bits and limits.
 *
 * This is the interface of the kernels: a layout does not change once
 * released. A kernel that needs different constants is a new kernel with its
 * own struct, and fields are added only by taking a reserved word.
 */

namespace MayaFlux::Yantra::TensorKernels {

/** @brief Invocations per workgroup of the element-wise kernels. */
inline constexpr uint32_t element_workgroup = 64;
/** @brief Invocations of the one workgroup nn_lstm_sequence runs in. */
inline constexpr uint32_t lstm_workgroup = 512;
/** @brief Widest hidden state nn_lstm_sequence supports. */
inline constexpr uint32_t lstm_max_hidden = 256;

inline constexpr const char* frame_audio_shader = "nn_frame_audio.comp.spv";
inline constexpr const char* conv1d_shader = "nn_conv1d.comp.spv";
inline constexpr const char* magnitude_shader = "nn_magnitude.comp.spv";
inline constexpr const char* lstm_sequence_shader = "nn_lstm_sequence.comp.spv";

/** @brief nn_conv1d flag bit: add the bias. */
inline constexpr uint32_t conv1d_flag_bias = 1;
/** @brief nn_conv1d flag bit: apply ReLU to the input. */
inline constexpr uint32_t conv1d_flag_relu_input = 2;
/** @brief nn_conv1d flag bits 2 and 3: the output activation, 0 none, 1 ReLU, 2 sigmoid. */
inline constexpr uint32_t conv1d_activation_shift = 2;

/** @brief Push constants of nn_conv1d. */
struct Conv1dPC {
    uint64_t src;
    uint64_t weights;
    uint64_t bias;
    uint64_t dst;
    uint32_t batch;
    uint32_t c_in;
    uint32_t t_in;
    uint32_t c_out;
    uint32_t t_out;
    uint32_t kernel_size;
    uint32_t stride;
    uint32_t pad;
    uint32_t flags;
    uint32_t dilation;
};

/** @brief Push constants of nn_frame_audio. */
struct FrameAudioPC {
    uint64_t samples;
    uint64_t frames;
    uint32_t chunks;
    uint32_t chunk_size;
    uint32_t context;
    uint32_t pad;
};

/** @brief Push constants of nn_magnitude. */
struct MagnitudePC {
    uint64_t src;
    uint64_t dst;
    uint32_t batch;
    uint32_t channels;
    uint32_t time;
    uint32_t reserved;
};

/** @brief Push constants of nn_lstm_sequence. */
struct LstmPC {
    uint64_t x;
    uint64_t w_ih;
    uint64_t w_hh;
    uint64_t b_ih;
    uint64_t b_hh;
    uint64_t state;
    uint64_t hs;
    uint32_t steps;
    uint32_t hidden;
};

static_assert(sizeof(Conv1dPC) == 72);
static_assert(sizeof(FrameAudioPC) == 32);
static_assert(sizeof(MagnitudePC) == 32);
static_assert(sizeof(LstmPC) == 64);

} // namespace MayaFlux::Yantra::TensorKernels
